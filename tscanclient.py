#!/usr/bin/env python3
# Simple client example application to talk to the T-scan clamservice
import os
import time
from typing import Iterable, Optional
from dataclasses import dataclass
import requests
from xml.etree import ElementTree

username = input("👤 Username: ")
password = input("🔒 Password: ")
address = "https://tscan.hum.uu.nl/tscan"

@dataclass
class Project:
    name: str
    status: int # 0 = staging 1 = scanning 2 = done
    statusmsg: str
    completion: int
    time: Optional[str] = None
    size: Optional[float] = None


def get_projects() -> Iterable[Project]:
    session = requests.Session()
    session.auth = (username, password)
    response = session.get(address + '/index')
    tree = ElementTree.fromstring(response.content)
    projects = tree.iterfind('*/project')
    for project in projects:
        attrib = project.attrib
        href = attrib['{http://www.w3.org/1999/xlink}href']
        yield Project(
             name=href.replace(f'{address}/', ''),
             time=attrib['time'],
             size=attrib['size'],
             status=attrib['status'],
             statusmsg='',
             completion=0)


def get_project(project: str) -> Project:
    session = requests.Session()
    session.auth = (username, password)
    response = session.get(f'{address}/{project}')
    tree = ElementTree.fromstring(response.content)
    access_token = tree.attrib['accesstoken']

    data = requests.get(f'{address}/{project}/status/?accesstoken={access_token}&user={username}').json()
    return Project(
        name=project,
        status=data['statuscode'],
        statusmsg=data['statusmsg'],
        completion=data['completion']
    )


def create_project(name: str) -> bool:
    session = requests.Session()
    session.auth = (username, password)
    return session.put(address + '/' + name).status_code == 200


def delete_project(name: str) -> bool:
    session = requests.Session()
    session.auth = (username, password)
    return session.delete(address + '/' + name).status_code == 200


def add_input(project: str, name: str, contents: str) -> bool:
    session = requests.Session()
    session.auth = (username, password)
    response = session.post(
        f"{address}/{project}/input/{name}.txt",
        params={
            'inputtemplate': 'textinput',
            'contents': contents
        })

    return response.status_code == 200


def scan(project: str) -> bool:
    session = requests.Session()
    session.auth = (username, password)
    return session.post(
        f'{address}/{project}',
        params={
            'overlapSize': '50',
            'frequencyClip': '99.0',
            'mtldThreshold': '0.72',
            'useAlpino': 'yes',
            'useWopr': 'no',
            'sentencePerLine': 'no',
            'prevalence': 'nl',
            'word_freq_lex': 'subtlex_words.freq',
            'lemma_freq_lex': 'freqlist_staphorsius_CLIB_lemma.freq',
            'top_freq_lex': 'SoNaR500.wordfreqlist20000.freq'
        }).ok

def get_project_filenames(project: str) -> Iterable[str]:
    session = requests.Session()
    session.auth = (username, password)
    response = session.get(f'{address}/{project}')
    tree = ElementTree.fromstring(response.content)
    filenames = tree.iterfind('output/file/name')

    for filename in filenames:
        yield filename.text

def save_output_file(project: str, filename: str) -> None:
    session = requests.Session()
    session.auth = (username, password)
    response = session.get(f'{address}/{project}/output/{filename}')

    os.makedirs(os.path.join('output', project), exist_ok=True)
    with open(os.path.join('output', project, filename), 'wb') as target:
        target.write(response.content)


def save_output_zip(project: str) -> None:
    session = requests.Session()
    session.auth = (username, password)
    response = session.get(f'{address}/{project}/output/zip')

    os.makedirs('output', exist_ok=True)
    with open(os.path.join('output', f'{project}.zip'), 'wb') as target:
        target.write(response.content)

#
#
# The functions above can now be used to manage T-scan
# Example below:
#
#

print(f"List of {username}'s projects:\n")
for project in get_projects():
    print(project.name)

project_name = input("\n\nNew project name? ")
if create_project(project_name):
    print("Created project!")

success = add_input(project_name, "example1", """Dit is een test.
En hier is nog een zin.""")

success |= add_input(project_name, "example2", """Hier is wat meer tekst.
En dan nog een zin!""")

if not success:
    print('Could not create example files')
    exit(1)

scan(project_name)

current_completion = -1
current_statusmsg = ''
current_status = -1

# Wait for the project to be scanned
while True:
    project = get_project(project_name)
    if project.completion != current_completion or \
        project.statusmsg != current_statusmsg or \
        project.status != current_status:
        current_completion = project.completion
        current_statusmsg = project.statusmsg
        current_status = project.status

        print(f'Scanning {current_completion}% STATUS={current_status} MSG={current_statusmsg}')

    if project.status == 2 or project.completion >= 100:
        break
    time.sleep(10)

# Download the results!

# Maybe this step will fail for very large projects?
# Directly download the files you need below
# You will probably know how they should be called
print("Downloading files...")
filenames = get_project_filenames(project_name)
for file in filenames:
    print(file)
    save_output_file(project_name, file)

# This should contain the same contents, but conveniently downloaded as a single
# ZIP-file... this will probably work better for projects with
# many small files
print("Downloading zip file")
save_output_zip(project_name)
print("DONE! 🎉")
while True:
    should_delete = input("Delete project (y/N)? ")
    if not should_delete or should_delete.strip().lower() == "n":
        break
    elif should_delete.strip().lower() == "y":
        delete_project(project_name)
        break
