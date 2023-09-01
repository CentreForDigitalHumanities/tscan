#!/usr/bin/env python3
from os import environ, listdir, path, system, remove, getpid
import json
from clam.common import status
import clam.clamservice
import tscanservice.tscan

CLAM_ROOT = environ.get('CLAM_ROOT') or "/data/www-data"
environ['CONFIGFILE'] = "/deployment/clam_custom.config.yml"

globals()['settings'] = tscanservice.tscan

clam.clamservice.settingsmodule = "tscan"


# Restart projects which were being processed whilst the T-Scan container stopped
PROJECTS_ROOT = f"{CLAM_ROOT}/tscan.clam/projects"
TSCAN_SERVICE_PATH = "/src/tscan/webservice/tscanservice"


def exit_status(project, user):
    done_path = path.join(PROJECTS_ROOT, user, project, ".done")
    try:
        with open(done_path) as f:
            return int(f.read(1024))
    except FileNotFoundError:
        # did not start
        return 1


# queue of all the projects to restart
# first they should be scheduled so the user knows ASAP what is going on
queue = []

for user in listdir(PROJECTS_ROOT):
    index_filename = path.join(PROJECTS_ROOT, user, ".index")
    try:
        with open(index_filename, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except FileNotFoundError:
        print(f"[restart-projects] .index not found for {user}")
        continue
    except ValueError:
        print(f"[restart-projects] malformed .index for {user}")
        continue

    for project in data['projects']:
        # tries to detect projects which were running when the server stopped
        if project[-1] == status.RUNNING or \
                (project[-1] == status.DONE and exit_status(project[0], user) != 0):
            print(
                f'[restart-projects] SCHEDULE RESTART OF PROJECT "{project[0]}" FOR "{user}"')
            project[-1] = status.RUNNING
            PROJECT_PATH = path.join(PROJECTS_ROOT, user, project[0])
            # remove intermediate files
            for file in [".done", ".aborted", ".status", "out.alpino_lookup.data", "problems.log"]:
                try:
                    remove(path.join(PROJECT_PATH, file))
                except FileNotFoundError:
                    pass

            with open(path.join(PROJECT_PATH, ".pid"), "w") as f:
                f.write(str(getpid()))

            clam.common.status.write(
                path.join(PROJECT_PATH, ".status"), "Scheduled for restart", 0)

            DATAFILE = path.join(PROJECT_PATH, "clam.xml")
            STATUSFILE = path.join(PROJECT_PATH, ".status")
            INPUTDIRECTORY = path.join(PROJECT_PATH, "input")
            OUTPUTDIRECTORY = path.join(PROJECT_PATH, "output")
            TSCAN_DIR = environ.get('TSCAN_DIR') or "/usr/local/bin"
            TSCAN_DATA = environ.get('TSCAN_DATA') or "/usr/local/share/tscan"
            TSCAN_SRC = environ.get('TSCAN_SRC') or "/src/tscan"
            ALPINO_HOME = environ.get('ALPINO_HOME') or "/Alpino"

            COMMAND = path.join(TSCAN_SERVICE_PATH, "tscanwrapper.py") + f" {DATAFILE} {STATUSFILE} {INPUTDIRECTORY} {OUTPUTDIRECTORY} " + \
                f"{TSCAN_DIR}  {TSCAN_DATA} {TSCAN_SRC} {ALPINO_HOME}"

            queue.append((
                f"clamdispatcher {TSCAN_SERVICE_PATH} tscanservice.tscan {PROJECT_PATH} {COMMAND}",
                project[0],
                user))

    with open(index_filename, 'w', encoding='utf-8') as f:
        json.dump(data, f)


for command, project_name, user in queue:
    try:
        system(command)
    except Exception as error:
        print(
            f'[restart-projects] FAILED TO RESTART PROJECT "{project_name}" FOR USER "{user}"')
        print(error)

print(f"""###
[restart-projects] FINISHED RESTARTING {len(queue)} PROJECTS
""")
