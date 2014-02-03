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

static string
function_name (Function *function)
{
  auto &names = function->getAllTypedNames();
  return !names.empty() ? names[0] : "(null)";
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
          auto &names = function->getAllTypedNames();
          if (!names.empty())
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
