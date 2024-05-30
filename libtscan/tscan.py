#!/usr/bin/env python3
import re
from typing import List, Union
from os import chdir, listdir, remove, rename, path, system
import sys
import shlex
import socket
import tempfile
from frog import Frog, FrogOptions
import folia.main as folia


# allow connecting to an external Frog server
class FrogClient:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port

    def process_raw(self, text: str) -> str:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((self.host, self.port))
            s.send((text + "\nEOT\n").encode("utf-8"))
            output: List[str] = []
            receiving = True
            while receiving:
                data = s.recv(1024)
                if not data:
                    break

                lines = data.decode("utf-8").splitlines(keepends=True)
                for line in lines:
                    if line.strip() == "READY":
                        receiving = False
                        break
                    else:
                        output.append(line)

            return ''.join(output)


frog_client: Union[Frog, FrogClient] = None


def connect_frog(host: str, port: int) -> None:
    global frog_client
    frog_client = FrogClient(host, port)


def get_frog_client() -> Union[Frog, FrogClient]:
    global frog_client
    if frog_client is None:
        frog_client = Frog(
            FrogOptions(xmlout=True, docid="tscan".encode(), mwu=False, parser=False)
        )
    return frog_client


def file_to_folia(filename: str, sentence_per_line: bool) -> folia.Document:
    comments = []
    text = ""
    with open(filename, mode="r", encoding="utf-8-sig") as file:
        lineno = -1
        while True:
            line = file.readline()
            if not line:
                break

            lineno += 1
            line = line.rstrip()

            # cut off line after ###
            match = line.find(" ### ")

            if match >= 0:
                line = line[0:match]

            # replace brackets
            line = re.sub(r"[\{\[]", "(", line)
            line = re.sub(r"[\}\]]", ")", line)

            if len(line) > 2:
                start = line[0:3]
                if start == "<<<":
                    if comments:
                        raise SyntaxError(
                            "Nested comment (<<<) not allowed!",
                            (
                                filename,
                                lineno,
                                0,
                                line,
                                "\n".join(comments + [line]),
                                lineno + 1,
                                3,
                            ),
                        )
                    else:
                        comments = [line]
                elif start == ">>>":
                    if not comments:
                        raise SyntaxError(
                            "end of comment (>>>) found without start.",
                            (filename, lineno, 0, line, text, lineno + 1, 3),
                        )
                    else:
                        comments = []
                        continue

            if comments:
                continue
            if sentence_per_line:
                text += line + "\n\n"
            else:
                text += line

    return text_to_folia(text)


def text_to_folia(text: str):
    client = get_frog_client()
    result = client.process_raw(text)

    return folia.Document(string=result)


def analyse(
    filename: str,
    output_dir: str,
    folia_document: folia.Document,
    config_file: str,
    skip_alpino=False,
    skip_csv_output=False,
    skip_wopr=False,
) -> int:
    config_file = path.abspath(config_file)
    command = ["tscan", "-F", f"--config={shlex.quote(config_file)}"]

    skip = ""
    if skip_alpino:
        skip += "a"
    if skip_csv_output:
        skip += "c"
    if skip_wopr:
        skip += "w"

    if skip:
        command += [f"--skip={skip}"]

    # TODO: store Alpino lookup, and Alpino parses
    with tempfile.TemporaryDirectory() as working_dir:
        filepath = path.join(working_dir, filename + ".xml")
        folia_document.save(filepath)
        command += [filepath]
        try:
            # set base path from config file as working directory
            chdir(path.dirname(config_file))
            exit_status = system(" ".join(command))
        except Exception as error:
            exit_status = 1
            print(error, file=sys.stderr)

        # discard the generated FoLiA which was used as input file
        remove(filepath)

        for output in listdir(working_dir):
            working_path = path.join(working_dir, output)
            # the filename plus all the extensions which have been added
            # by T-scan
            renamed = filename + working_path[len(filepath) :]
            rename(working_path, path.join(output_dir, renamed))

    return exit_status
