import sys
import os
from unreal_engine.classes import BlueprintPathsLibrary

def attach_to_debugger(host='localhost', port=51224):
    try:
        # TODO : Use your PyCharm install directory.
        #pydev_path = "D:/PyCharm/PyCharm 2018.3.4/helpers/pydev"
        pydev_path = os.path.join(os.path.dirname(__file__), "pydevd-pycharm.egg")
        if not pydev_path in sys.path:
            sys.path.append(pydev_path)
        import pydevd
        pydevd.stoptrace()
        pydevd.settrace(
            port=port,
            host=host,
            stdoutToServer=True,
            stderrToServer=True,
            overwrite_prev_trace=True,
            suspend=False,
            trace_only_current_thread=False,
            patch_multiprocessing=False,
        )
        print("PyCharm Remote Debug enabled on %s:%s." % (host, port))
    except:
        import traceback
        traceback.print_exc()

def detach_debugger():
    try:
        import pydevd
        pydevd.stoptrace()
    except:
        import traceback
        traceback.print_exc()

#attach_to_debugger('localhost', 51224)