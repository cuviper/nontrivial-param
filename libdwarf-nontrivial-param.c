#include <demangle.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dwarf.h>
#include <libdwarf.h>


typedef struct CU_Data_s
{
  char **srcfiles;
  Dwarf_Unsigned nsrcfiles;
} *CU_Data;


static Dwarf_Half
dwarf_tag_simple (Dwarf_Die die)
{
  Dwarf_Half tag = -1;
  dwarf_tag (die, &tag, NULL);
  return tag;
}

static Dwarf_Bool
dwarf_hasattr_simple (Dwarf_Die die, Dwarf_Half attr)
{
  Dwarf_Bool hasattr = 0;
  dwarf_hasattr (die, attr, &hasattr, NULL);
  return hasattr;
}

static Dwarf_Die
dwarf_attr_die (Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr)
{
  Dwarf_Die attr_die = NULL;
  Dwarf_Attribute attr_val;
  if (dwarf_attr (die, attr, &attr_val, NULL) == DW_DLV_OK)
    {
      Dwarf_Off offset;
      if (dwarf_global_formref (attr_val, &offset, NULL) == DW_DLV_OK)
        dwarf_offdie (dbg, offset, &attr_die, NULL);
      dwarf_dealloc (dbg, attr_val, DW_DLA_ATTR);
    }
  return attr_die;
}

static Dwarf_Unsigned
dwarf_attr_unum (Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr)
{
  Dwarf_Unsigned unum = UINT64_MAX;
  Dwarf_Attribute attr_val;
  if (dwarf_attr (die, attr, &attr_val, NULL) == DW_DLV_OK)
    {
      if (dwarf_formudata (attr_val, &unum, NULL) != DW_DLV_OK)
        {
          Dwarf_Signed snum;
          if (dwarf_formsdata (attr_val, &snum, NULL) == DW_DLV_OK)
            unum = snum;
        }
      dwarf_dealloc (dbg, attr_val, DW_DLA_ATTR);
    }
  return unum;
}

static Dwarf_Attribute
dwarf_attr_integrate (Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr)
{
  Dwarf_Attribute attr_val = NULL;
  if (dwarf_attr (die, attr, &attr_val, NULL) != DW_DLV_OK)
    {
      Dwarf_Die origin = dwarf_attr_die (dbg, die, DW_AT_abstract_origin)
        ?: dwarf_attr_die (dbg, die, DW_AT_specification);
      if (origin)
        {
          attr_val = dwarf_attr_integrate (dbg, origin, attr);
          dwarf_dealloc (dbg, origin, DW_DLA_DIE);
        }
    }
  return attr_val;
}

static char *
dwarf_diename_integrate (Dwarf_Debug dbg, Dwarf_Die die)
{
  char *name = NULL;
  Dwarf_Attribute attr = dwarf_attr_integrate (dbg, die, DW_AT_name);
  if (attr)
    dwarf_formstring (attr, &name, NULL);
  return name;
}

static const char *
dwarf_decl_file (Dwarf_Debug dbg, CU_Data cudata, Dwarf_Die die)
{
  const char *file = NULL;
  Dwarf_Unsigned file_index = dwarf_attr_unum (dbg, die, DW_AT_decl_file);
  if (file_index > 0 && file_index <= cudata->nsrcfiles)
    file = cudata->srcfiles[file_index - 1];
  return file;
}

static bool
has_nontrivial_type (Dwarf_Debug dbg, Dwarf_Die die)
{
  bool ret = false;

  Dwarf_Die type = dwarf_attr_die (dbg, die, DW_AT_type);
  if (type)
    {
      switch (dwarf_tag_simple (type))
        {
        case DW_TAG_class_type:
        case DW_TAG_structure_type:
          ret = true;
          break;
        case DW_TAG_const_type:
        case DW_TAG_volatile_type:
        case DW_TAG_typedef:
          ret = has_nontrivial_type (dbg, type);
          break;
        }
      dwarf_dealloc (dbg, type, DW_DLA_DIE);
    }

  return ret;
}

static char*
function_name (Dwarf_Debug dbg, Dwarf_Die function)
{
  char *name = NULL;

  bool mangled = false;
  Dwarf_Attribute attr =
    dwarf_attr_integrate (dbg, function, DW_AT_linkage_name) ?:
    dwarf_attr_integrate (dbg, function, DW_AT_MIPS_linkage_name);
  if (attr)
    mangled = true;
  else
    attr = dwarf_attr_integrate (dbg, function, DW_AT_name);

  if (attr)
    {
      char *formname;
      if (dwarf_formstring (attr, &formname, NULL) == DW_DLV_OK)
        {
          name = mangled ? cplus_demangle (formname, DMGL_PARAMS)
            : strdup (formname);
          dwarf_dealloc (dbg, formname, DW_DLA_STRING);
        }
      dwarf_dealloc (dbg, attr, DW_DLA_ATTR);
    }

  return name ?: strdup ("(null)");
}

static bool
in_system_header (const char *file)
{
  return strncmp (file, "/usr/", 5) == 0 &&
    strncmp (file, "/usr/src/debug/", 15) != 0;
}

static void
process_function (Dwarf_Debug dbg, CU_Data cudata, Dwarf_Die function)
{
  bool printed_function_name = false;

  const char *file = dwarf_decl_file (dbg, cudata, function);
  if (!file || in_system_header (file))
    return;

  Dwarf_Die child;
  if (dwarf_child (function, &child, NULL) == DW_DLV_OK)
    {
      for (;;)
        {
          if (dwarf_tag_simple (child) == DW_TAG_formal_parameter
              && has_nontrivial_type (dbg, child))
            {
              if (!printed_function_name)
                {
                  char *name = function_name (dbg, function);
                  fprintf (stderr, "%s: In function ‘%s’:\n", file, name);
                  free (name);
                  printed_function_name = true;
                }

              char *diename = dwarf_diename_integrate (dbg, child);
              if (diename)
                {
                  Dwarf_Unsigned line = dwarf_attr_unum (dbg, child, DW_AT_decl_line);
                  fprintf (stderr,
                           "%s:%" DW_PR_DUu ": note: parameter ‘%s’ type is not trivial\n",
                           dwarf_decl_file (dbg, cudata, child), line, diename);
                  dwarf_dealloc (dbg, diename, DW_DLA_STRING);
                }
            }

          Dwarf_Die sibling;
          if (dwarf_siblingof (dbg, child, &sibling, NULL) == DW_DLV_OK)
            {
              dwarf_dealloc (dbg, child, DW_DLA_DIE);
              child = sibling;
            }
          else
            break;
        }
      dwarf_dealloc (dbg, child, DW_DLA_DIE);
    }
}

static void
search_functions (Dwarf_Debug dbg, CU_Data cudata, Dwarf_Die die)
{
  Dwarf_Half tag = dwarf_tag_simple (die);
  if (tag == DW_TAG_subprogram &&
      !dwarf_hasattr_simple (die, DW_AT_declaration))
    process_function (dbg, cudata, die);

  if (tag == DW_TAG_imported_unit)
    {
      Dwarf_Die import = dwarf_attr_die (dbg, die, DW_AT_import);
      if (import)
        {
          search_functions (dbg, cudata, import);
          dwarf_dealloc (dbg, import, DW_DLA_DIE);
        }
    }

  Dwarf_Die child;
  if (dwarf_child (die, &child, NULL) == DW_DLV_OK)
    {
      for (;;)
        {
          search_functions (dbg, cudata, child);

          Dwarf_Die sibling;
          if (dwarf_siblingof (dbg, child, &sibling, NULL) == DW_DLV_OK)
            {
              dwarf_dealloc (dbg, child, DW_DLA_DIE);
              child = sibling;
            }
          else
            break;
        }
      dwarf_dealloc (dbg, child, DW_DLA_DIE);
    }
}

int
main (int argc, char *argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;

  int fd = open (argv[1], O_RDONLY);
  if (fd < 0)
    error (EXIT_FAILURE, errno, "open");

  Dwarf_Debug dbg;
  if (dwarf_init (fd, DW_DLC_READ, NULL, NULL, &dbg, NULL) != DW_DLV_OK)
    {
      close (fd);
      error (EXIT_FAILURE, 0, "dwarf_init failed");
    }

  Dwarf_Unsigned next_cu = 0;
  while (dwarf_next_cu_header (dbg, NULL, NULL, NULL, NULL,
                               &next_cu, NULL) == DW_DLV_OK)
    {
      Dwarf_Die cudie;
      if (dwarf_siblingof (dbg, NULL, &cudie, NULL) == DW_DLV_OK)
        {
          Dwarf_Half cutag;
          if (dwarf_tag (cudie, &cutag, NULL) == DW_DLV_OK
              && cutag == DW_TAG_compile_unit)
            {
              // libdwarf, why is this signed?  DW_AT_decl_file is unsigned...
              Dwarf_Signed nsrcfiles;
              struct CU_Data_s cudata;
              if (dwarf_srcfiles (cudie, &cudata.srcfiles,
                                  &nsrcfiles, NULL) == DW_DLV_OK
                  && nsrcfiles >= 0)
                {
                  cudata.nsrcfiles = (Dwarf_Unsigned) nsrcfiles;
                  search_functions (dbg, &cudata, cudie);

                  for (Dwarf_Unsigned i = 0; i < cudata.nsrcfiles; ++i)
                    dwarf_dealloc(dbg, cudata.srcfiles[i], DW_DLA_STRING);
                  dwarf_dealloc(dbg, cudata.srcfiles, DW_DLA_LIST);
                }
            }
          dwarf_dealloc (dbg, cudie, DW_DLA_DIE);
        }
    }

  dwarf_finish (dbg, NULL);
  close (fd);

  return EXIT_SUCCESS;
}
