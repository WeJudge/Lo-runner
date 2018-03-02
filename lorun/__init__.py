import platform
import importlib

def unsupport(*args, **kwargs):
    print("Sorry, Lo-runner can only run in Linux.")

if 'linux' in platform.system().lower():
    try:
        lorun = importlib.import_module('lorun')
        run = lorun.run
        run_interactive = lorun.run_interactive
        run_checker = lorun.run_checker
        check = lorun.check
    except Exception as ex:
        from ._lorun_ext import run, run_interactive, run_checker, check
else:
    run = run_interactive = run_checker = check = unsupport