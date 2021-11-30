#!/usr/bin/env python
# -*- coding:utf-8 -*-

###############################################################
# CLAM: Computational Linguistics Application Mediator
# -- Service Configuration File --
#       by Maarten van Gompel (proycon)
#       http://ilk.uvt.nl/~mvgompel
#       Induction for Linguistic Knowledge Research Group
#       Universiteit van Tilburg
#
#       Licensed under GPLv3
#
###############################################################

from clam.common.parameters import *
from clam.common.formats import *
from clam.common.converters import *
from clam.common.viewers import *
from clam.common.data import *  # AbstractConverter

import os
from base64 import b64decode as D
import glob

from .text_converter import text_convert, use_text_converter

try:
    from compound_splitter.splitter import list_methods
    compound_methods = list(method["name"] for method in list_methods())
except ModuleNotFoundError:
    compound_methods = None

# DEBUG = True


class DocumentTextConverter(AbstractConverter):
    acceptforinput = [clam.common.formats.PlainTextFormat]

    converttool = 'textract'

    def __init__(self, id,  **kwargs):
        super(DocumentTextConverter, self).__init__(id, **kwargs)

    def convertforinput(self, filepath, metadata=None):
        super(DocumentTextConverter, self).convertforinput(filepath, metadata)
        return text_convert(filepath)


REQUIRE_VERSION = 2.3
CLAMDIR = clam.__path__[0]

# ======== GENERAL INFORMATION ===========

# General information concerning your system.


# The System ID, a short alphanumeric identifier for internal use only
SYSTEM_ID = "tscan"

# System name, the way the system is presented to the world
SYSTEM_NAME = "T-scan"

SYSTEM_AUTHOR = "Ko van der Sloot, Martijn van der Klis, Luka van der Plas, Sheean Spoel, Maarten van Gompel"

SYSTEM_AFFILIATION = "Utrecht University"

# An informative description for this system (this should be fairly short, about one paragraph, and may not contain HTML)
SYSTEM_DESCRIPTION = "T-Scan is an analysis tool for Dutch text, mainly focusing on text complexity. It has been initially conceptualized by Rogier Kraf and Henk Pander Maat. Rogier Kraf also programmed the first versions. From 2012 on, Henk Pander Maat supervised the development of the extended versions of the tool. These versions were programmed by Maarten van Gompel, Ko van der Sloot, Martijn van der Klis, Sheean Spoel and Luka van der Plas."

SYSTEM_URL = "https://github.com/proycon/tscan"

SYSTEM_EMAIL = "s.j.j.spoel@uu.nl"

SYSTEM_LICENSE = "GNU Affero General Public License v3"


# ======== LOCATION ===========

USERS = None


if 'ALPINO_HOME' in os.environ:
    ALPINOHOME = os.environ['ALPINO_HOME']
else:
    ALPINOHOME = ""

SWITCHBOARD_FORWARD_URL = None

# load external configuration file (see tscan.config.yml)
loadconfig(__name__)


# If the webservice runs in another webserver (e.g. apache, nginx, lighttpd), and it
# doesn't run at the root of the server, you can specify a URL prefix here:
# URLPREFIX = "/myservice/"

# The location of where CLAM is installed (will be determined automatically if not set)
# CLAMDIR = "/path/to/clam"

# ======== AUTHENTICATION & SECURITY ===========

# Users and passwords

# set security realm, a required component for hashing passwords (will default to SYSTEM_ID if not set)
# REALM = SYSTEM_ID

# If you want to enable user-based security, you can define a dictionary
# of users and (hashed) passwords here. The actual authentication will proceed
# as HTTP Digest Authentication. Although being a convenient shortcut,
# using pwhash and plaintext password in this code is not secure!!

# USERS = { user1': '4f8dh8337e2a5a83734b','user2': pwhash('username', REALM, 'secret') }

# Amount of free memory required prior to starting a new process (in MB!), Free Memory + Cached (without swap!). Set to 0 to disable this check (not recommended)
# REQUIREMEMORY = 10

# Maximum load average at which processes are still started (first number reported by 'uptime'). Set to 0 to disable this check (not recommended)
# MAXLOADAVG = 4.0

# Minimum amount of free diskspace in MB. Set to 0 to disable this check (not recommended)
# DISK = '/dev/sda1' #set this to the disk where ROOT is on
# MINDISKSPACE = 10


# ======== WEB-APPLICATION STYLING =============

# Choose a style (has to be defined as a CSS file in style/ ). You can copy, rename and adapt it to make your own style
STYLE = 'classic'

# ======== ENABLED FORMATS ===========

# Here you can specify an extra formats module
CUSTOM_FORMATS_MODULE = None

# ======== PREINSTALLED DATA ===========

# INPUTSOURCES = [
#    InputSource(id='sampledocs',label='Sample texts',path=ROOT+'/inputsources/sampledata',defaultmetadata=PlainTextFormat(None, encoding='utf-8') ),
# ]

CUSTOMHTML_INDEX = CUSTOMHTML_PROJECTSTART = "<p>Please consult the <a href=\"https://github.com/proycon/tscan/raw/master/docs/tscanhandleiding.pdf\">Handleiding</a> when using T-Scan.</p>"

# ======== PROFILE DEFINITIONS ===========

PROFILES = [
    Profile(
        InputTemplate(
            'textinput',
            PlainTextFormat,
            'Text Input',
            StaticParameter(id='encoding', name='Encoding',
                            description='The character encoding of the file', value='utf-8'),
            DocumentTextConverter(
                id='docconv', label='Convert from DOC/DOCX/ODT') if use_text_converter else None,
            # PDFtoTextConverter(id='pdfconv',label='Convert from PDF Document'),
            # MSWordConverter(id='docconv',label='Convert from MS Word Document'),
            extension='.txt',
            # filename='filename.txt',
            # unique=True #set unique=True if the user may only upload a file for this input template once. Set multi=True if you the user may upload multiple of such files
            acceptarchive=True,
            multi=True
        ),
        # 20180305: Added possibility to add a stoplist
        InputTemplate(
            'stoplist',
            PlainTextFormat,
            'Stoplist',
            StaticParameter(id='encoding', name='Encoding',
                            description='The character encoding of the file', value='utf-8'),
            extension='.data',
            optional=True,
            unique=True,
        ),
        # 20160802: Added possibility to enter your own classification
        InputTemplate(
            'myclassification',
            PlainTextFormat,
            'Own Classification',
            StaticParameter(id='encoding', name='Encoding',
                            description='The character encoding of the file', value='utf-8'),
            extension='.data',
            optional=True,
            unique=True,
        ),
        # 20150316: Added possibility to enter custom adjective classification
        InputTemplate(
            'adjclassification',
            PlainTextFormat,
            'Adjective Classification',
            StaticParameter(id='encoding', name='Encoding',
                            description='The character encoding of the file', value='utf-8'),
            extension='.data',
            optional=True,
            unique=True,
        ),
        # 20141121: Added possibility to enter custom noun classification
        InputTemplate(
            'nounclassification',
            PlainTextFormat,
            'Noun Classification',
            StaticParameter(id='encoding', name='Encoding',
                            description='The character encoding of the file', value='utf-8'),
            extension='.data',
            optional=True,
            unique=True,
        ),
        # 20150203: Added possibility to enter custom intensifying words
        InputTemplate(
            'intensify',
            PlainTextFormat,
            'Intensifying words',
            StaticParameter(id='encoding', name='Encoding',
                            description='The character encoding of the file', value='utf-8'),
            extension='.data',
            optional=True,
            unique=True,
        ),
        # ------------------------------------------------------------------------------------------------------------------------
        OutputTemplate(
            'foliaout',
            FoLiAXMLFormat,
            'Output analysis',
            XSLTViewer(file=TSCANDIR + '/view/tscanview.xsl'),
            ForwardViewer(id='switchboardforwarder', name="Open in CLARIN Switchboard", forwarder=Forwarder(
                'switchboard', 'CLARIN Switchboard', SWITCHBOARD_FORWARD_URL), allowdefault=False) if SWITCHBOARD_FORWARD_URL else None,
            removeextension='.txt',  # remove prior to adding
            extension='.xml',
            multi=True
        ),
        OutputTemplate(
            'xsl',
            XMLStyleSheet,
            'Stylesheet for Visualisation',
            filename='tscanview.xsl',
            unique=True
        ),
        OutputTemplate(
            'wordcsv',
            CSVFormat,
            'Document statistics, per word',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            extension='words.csv',
            multi=True
        ),
        OutputTemplate(
            'sencsv',
            CSVFormat,
            'Document statistics, per sentence',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            extension='sentences.csv',
            multi=True
        ),
        OutputTemplate(
            'parcsv',
            CSVFormat,
            'Document statistics, per paragraph',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            extension='paragraphs.csv',
            multi=True
        ),
        OutputTemplate(
            'doccsv',
            CSVFormat,
            'Document statistics, entire document',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            extension='document.csv',
            multi=True
        ),
        OutputTemplate(
            'totalwordcsv',
            CSVFormat,
            'Aggregated statistics, per word',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            filename='total.word.csv',
            unique=True
        ),
        OutputTemplate(
            'totalsencsv',
            CSVFormat,
            'Aggregated statistics, per sentence',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            filename='total.sen.csv',
            unique=True
        ),
        OutputTemplate(
            'totalparcsv',
            CSVFormat,
            'Aggregated statistics, per paragraph',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            filename='total.par.csv',
            unique=True
        ),
        OutputTemplate(
            'totaldoccsv',
            CSVFormat,
            'Aggregated statistics, per document',
            SetMetaField('encoding', 'utf-8'),
            SimpleTableViewer(delimiter=",", quotechar='"'),
            filename='total.doc.csv',
            unique=True
        )
    )
]

# ======== COMMAND ===========

# The system command. It is recommended you set this to small wrapper
# script around your actual system. Full shell syntax is supported. Using
# absolute paths is preferred. The current working directory will be
# set to the project directory.
#
# You can make use of the following special variables,
# which will be automatically set by CLAM:
#     $INPUTDIRECTORY  - The directory where input files are uploaded.
#     $OUTPUTDIRECTORY - The directory where the system should output
#                        its output files.
#     $STATUSFILE      - Filename of the .status file where the system
#                        should output status messages.
#     $DATAFILE        - Filename of the clam.xml file describing the
#                        system and chosen configuration.
#     $USERNAME        - The username of the currently logged in user
#                        (set to "anonymous" if there is none)
#     $PARAMETERS      - List of chosen parameters, using the specified flags
#
COMMAND = TSCANDIR + "/webservice/tscanservice/tscanwrapper.py $DATAFILE $STATUSFILE $INPUTDIRECTORY $OUTPUTDIRECTORY " + \
    TSCANDIR + " " + ALPINOHOME

# ======== PARAMETER DEFINITIONS ===========

# The parameters are subdivided into several groups. In the form of a list of (groupname, parameters) tuples. The parameters are a list of instances from common/parameters.py

wordfreqlist = []
for f in glob.glob(TSCANDIR + "/data/*words.freq"):
    wordfreqlist.append((os.path.basename(f), os.path.basename(f)))

lemmafreqlist = []
for f in glob.glob(TSCANDIR + "/data/*lemma.freq"):
    lemmafreqlist.append((os.path.basename(f), os.path.basename(f)))

topfreqlist = []
for f in glob.glob(TSCANDIR + "/data/*20000.freq"):
    topfreqlist.append((os.path.basename(f), os.path.basename(f)))


parameters_list = [
    IntegerParameter(id='overlapSize', name='Overlap Size',
                     description='Overlap Size', default=50),
    FloatParameter(id='frequencyClip', name='Frequency Clipping',
                   description='Frequency Clipping', default=99),
    FloatParameter(id='mtldThreshold', name='MTLD factor size',
                   description='MTLD factor size', default=0.720),
    ChoiceParameter(id='useAlpino', name='Use Alpino parser',
                    description='Use Alpino parser?', choices=['yes', 'no'], default='yes'),
    ChoiceParameter(id='useWopr', name='Use Wopr', description='Use Wopr?', choices=[
        'yes', 'no'], default='yes'),
    ChoiceParameter(id='sentencePerLine', name='One sentence per line',
                    description='Are the input texts already split per line?', choices=['yes', 'no'], default='no'),
    ChoiceParameter(id='prevalence', name='Prevalence data', description='Use prevalence data (http://crr.ugent.be/programs-data/word-prevalence-values) for',
                    choices=[('nl', 'The Netherlands'), ('be', 'Belgium')], default='nl'),
    ChoiceParameter(id='word_freq_lex', name='Word Frequency List', description="Word frequency list",
                    choices=wordfreqlist, default="SoNaR500.wordfreqlist_words.freq"),
    ChoiceParameter(id='lemma_freq_lex', name='Lemma Frequency List', description="Lemma frequency list",
                    choices=lemmafreqlist, default="SoNaR500.wordfreqlist_lemma.freq"),
    ChoiceParameter(id='top_freq_lex', name='Top Frequency List', description="Top frequency list",
                    choices=topfreqlist, default="SoNaR500.wordfreqlist20000.freq")
]

if compound_methods != None:
    parameters_list.append(
        ChoiceParameter(id='compoundSplitterMethod', name='Compound split method', description='Method used by compound splitting module',
                        choices=compound_methods, default='secos'))

PARAMETERS = [
    ('Parameters', parameters_list)
]


# ======== DISPATCHING (ADVANCED! YOU CAN SAFELY SKIP THIS!) ========

# The dispatcher to use (defaults to clamdispatcher.py), you almost never want to change this
# DISPATCHER = 'clamdispatcher.py'

# DISPATCHER_POLLINTERVAL = 30   #interval at which the dispatcher polls for resource consumption (default: 30 secs)
# DISPATCHER_MAXRESMEM = 0    #maximum consumption of resident memory (in megabytes), processes that exceed this will be automatically aborted. (0 = unlimited, default)
# DISPATCHER_MAXTIME = 0      #maximum number of seconds a process may run, it will be aborted if this duration is exceeded.   (0=unlimited, default)
# DISPATCHER_PYTHONPATH = []        #list of extra directories to add to the python path prior to launch of dispatcher

# Run background process on a remote host? Then set the following (leave the lambda in):
# REMOTEHOST = lambda: return 'some.remote.host'
# REMOTEUSER = 'username'

# For this to work, the user under which CLAM runs must have (passwordless) ssh access (use ssh keys) to the remote host using the specified username (ssh REMOTEUSER@REMOTEHOST)
# Moreover, both systems must have access to the same filesystem (ROOT) under the same mountpoint.
