import gcc

def in_system_header(loc):
    if hasattr(loc, "in_system_header"):
        return loc.in_system_header
    # approximate it
    return (loc.file.startswith('/usr/') and
            not loc.file.startswith('/usr/src/debug/'))

def on_pass_execution(p, fn):
    # This pass is called fairly early on, per-function, after the
    # CFG has been built.  Skip every other pass.
    if p.name != '*warn_function_return':
        return

    # I don't want notices for system headers.
    if in_system_header(fn.decl.location):
        return

    for parm, parmtype in zip(fn.decl.arguments,
                              fn.decl.type.argument_types):
        if type(parmtype) == gcc.RecordType:
            gcc.inform(parm.location,
                       'parameter type is not trivial')

gcc.register_callback(gcc.PLUGIN_PASS_EXECUTION,
                      on_pass_execution)
