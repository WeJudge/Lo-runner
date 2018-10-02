import platform


def unsupport(*args, **kwargs):
    print("Sorry, Lo-runner can only run in Linux.")


def not_install(*args, **kwargs):
    print("Sorry, Lo-runner not installed.")


if 'linux' in platform.system().lower():
    try:
        try:
            from lorun._lorun_ext import run, run_interactive, run_checker, check
        except Exception as ex:
            from ._lorun_ext import run, run_interactive, run_checker, check
    except:
        run = run_interactive = run_checker = check = not_install

else:
    run = run_interactive = run_checker = check = unsupport