#include <Symtab.h>
#include <Function.h>
#include <Type.h>

using namespace std;
using namespace Dyninst::SymtabAPI;

static bool
has_nontrivial_type (Type *type)
{
  switch (type->getDataClass())
    {
    case dataStructure:
    case dataTypeClass:
      return true;
    case dataTypedef:
      return has_nontrivial_type
        (type->getTypedefType()->getConstituentType());
    default:
      return false;
    }
}

static bool
has_function_name (Function *function) {
  return function->typed_names_begin() != function->typed_names_end();
}

static string
function_name (Function *function)
{
  return has_function_name (function) ? *function->typed_names_begin() : "(null)";
}

static bool
in_system_header (const string& file)
{
  return file.compare (0, 5, "/usr/") == 0 &&
    file.compare (0, 15, "/usr/src/debug/") != 0;
}

static void
process_function (Function *function)
{
  vector<localVar*> parameters;
  if (!function->getParams (parameters) || parameters.empty() ||
      in_system_header (parameters[0]->getFileName()))
    return;

  bool printed_function_name = false;
  for (auto parameter : parameters)
    {
      if (!has_nontrivial_type (parameter->getType()))
        continue;

      if (!printed_function_name)
        {
          if (has_function_name (function))
            cerr << parameter->getFileName() << ": In function ‘"
                 << function_name (function) << "’:" << endl;
          printed_function_name = true;
        }

      cerr << parameter->getFileName() << ":" << parameter->getLineNum()
           << ": note: parameter ‘" << parameter->getName()
           << "’ type is not trivial" << endl;
    }
}

int
main (int argc, char *argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;

  Symtab *symtab;
  vector<Function*> functions;
  if (!Symtab::openFile (symtab, argv[1]) ||
      !symtab->getAllFunctions (functions))
    {
      auto err = Symtab::getLastSymtabError();
      clog << Symtab::printError (err) << endl;
      return EXIT_FAILURE;
    }

  for (auto function : functions)
    process_function (function);

  return EXIT_SUCCESS;
}
