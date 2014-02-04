#include <argp.h>
#include <demangle.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwarf.h>
#include <libdw.h>
#include <libdwfl.h>

static bool
has_nontrivial_type (Dwarf_Die *die)
{
  Dwarf_Die type;
  Dwarf_Attribute type_attr;
  if (dwarf_attr (die, DW_AT_type, &type_attr)
      && dwarf_formref_die (&type_attr, &type))
    switch (dwarf_tag (&type))
      {
      case DW_TAG_class_type:
      case DW_TAG_structure_type:
        return true;
      case DW_TAG_const_type:
      case DW_TAG_volatile_type:
      case DW_TAG_typedef:
        return has_nontrivial_type (&type);
      }
  return false;
}

static char*
function_name (Dwarf_Die *function)
{
  const char *linkage_name;
  Dwarf_Attribute linkage_attr;
  if ((dwarf_attr_integrate (function, DW_AT_linkage_name, &linkage_attr) ||
       dwarf_attr_integrate (function, DW_AT_MIPS_linkage_name, &linkage_attr))
      && (linkage_name = dwarf_formstring (&linkage_attr)))
    return cplus_demangle (linkage_name, DMGL_PARAMS);

  const char *name = dwarf_diename (function);
  return strdup (name ?: "(null)");
}

static bool
in_system_header (const char *file)
{
  return strncmp (file, "/usr/", 5) == 0 &&
    strncmp (file, "/usr/src/debug/", 15) != 0;
}

static int
process_function (Dwarf_Die *function, void *arg __attribute__ ((unused)))
{
  bool printed_function_name = false;
  const char *file = dwarf_decl_file (function);
  if (!file || in_system_header (file))
    return DWARF_CB_OK;

  Dwarf_Die child;
  if (dwarf_child (function, &child) == 0)
    do
      if (dwarf_tag (&child) == DW_TAG_formal_parameter
          && has_nontrivial_type (&child))
        {
          if (!printed_function_name)
            {
              char *name = function_name (function);
              fprintf (stderr, "%s: In function ‘%s’:\n", file, name);
              free (name);
              printed_function_name = true;
            }

          int line = -1;
          dwarf_decl_line (&child, &line);
          fprintf (stderr, "%s:%i: note: parameter ‘%s’ type is not trivial\n",
                   dwarf_decl_file (&child), line, dwarf_diename (&child));
        }
    while (dwarf_siblingof (&child, &child) == 0);

  return DWARF_CB_OK;
}

int
main (int argc, char *argv[])
{
  Dwfl *dwfl;

  // hack a single non-option argument into -e ARG
  if (argc == 2 && argv[1][0] != '-')
    {
      char *hacked_argv[3] = { argv[0], "-e", argv[1] };
      argp_parse (dwfl_standard_argp (), 3, hacked_argv, 0, NULL, &dwfl);
    }
  else
    argp_parse (dwfl_standard_argp (), argc, argv, 0, NULL, &dwfl);


  Dwarf_Die *cu = NULL;
  Dwarf_Addr dwbias;
  while ((cu = dwfl_nextcu (dwfl, cu, &dwbias)))
    if (dwarf_tag (cu) == DW_TAG_compile_unit)
      dwarf_getfuncs (cu, process_function, NULL, 0);

  dwfl_end (dwfl);
  return EXIT_SUCCESS;
}
