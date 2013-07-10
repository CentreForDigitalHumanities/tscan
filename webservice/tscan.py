#!/usr/bin/env python
#-*- coding:utf-8 -*-

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
from clam.common.data import *
from clam.common.digestauth import pwhash
import sys
from os import uname, environ
from base64 import b64decode as D
import glob

#DEBUG = True

REQUIRE_VERSION = 0.8

# ======== GENERAL INFORMATION ===========

# General information concerning your system.


#The System ID, a short alphanumeric identifier for internal use only
SYSTEM_ID = "tscan"

#System name, the way the system is presented to the world
SYSTEM_NAME = "tscan"

#An informative description for this system (this should be fairly short, about one paragraph, and may not contain HTML)
SYSTEM_DESCRIPTION = ""

# ======== LOCATION ===========

hostname = uname()[1]

if hostname == 'applejack': #final server in Nijmegen
    CLAMDIR = "/scratch2/www/webservices-lst/live/repo/clam"
    TSCANDIR = "/scratch2/www/webservices-lst/live/repo/tscan"
    ROOT = "/scratch2/www/webservices-lst/live/writable/tscan/"
    HOST = "webservices-lst.science.ru.nl"
    PORT = 80
    URLPREFIX = "tscan"
    USERS_MYSQL = {
        'host': 'mysql-clamopener.science.ru.nl',
        'user': 'clamopener',
        'password': D(open(environ['CLAMOPENER_KEYFILE']).read().strip()),
        'database': 'clamopener',
        'table': 'clamusers_clamusers',
	'accesslist': environ['accesslist'].split(' ')
    }
    REALM = "WEBSERVICES-LST"
else: #local
    TSCANDIR = os.path.dirname(sys.argv[0])
    ROOT = "/tmp/tscan.clam/"
    PORT= 8080
    USERS = None


    #Directory to the tscan root (svn checkout)


    #The URL of the system (If you start clam with the built-in webserver, you can override this with -P)


#The hostname of the system. Will be automatically determined if not set. (If you start clam with the built-in webserver, you can override this with -H)
#Users *must* make use of this hostname and no other (even if it points to the same IP) for the web application to work.
#HOST = 'localhost'

#If the webservice runs in another webserver (e.g. apache, nginx, lighttpd), and it
#doesn't run at the root of the server, you can specify a URL prefix here:
#URLPREFIX = "/myservice/"

#The location of where CLAM is installed (will be determined automatically if not set)
#CLAMDIR = "/path/to/clam"

# ======== AUTHENTICATION & SECURITY ===========

#Users and passwords

#set security realm, a required component for hashing passwords (will default to SYSTEM_ID if not set)
#REALM = SYSTEM_ID

#If you want to enable user-based security, you can define a dictionary
#of users and (hashed) passwords here. The actual authentication will proceed
#as HTTP Digest Authentication. Although being a convenient shortcut,
#using pwhash and plaintext password in this code is not secure!!

#USERS = { user1': '4f8dh8337e2a5a83734b','user2': pwhash('username', REALM, 'secret') }

#Amount of free memory required prior to starting a new process (in MB!), Free Memory + Cached (without swap!). Set to 0 to disable this check (not recommended)
#REQUIREMEMORY = 10

#Maximum load average at which processes are still started (first number reported by 'uptime'). Set to 0 to disable this check (not recommended)
#MAXLOADAVG = 4.0

#Minimum amount of free diskspace in MB. Set to 0 to disable this check (not recommended)
#DISK = '/dev/sda1' #set this to the disk where ROOT is on
#MINDISKSPACE = 10


# ======== WEB-APPLICATION STYLING =============

#Choose a style (has to be defined as a CSS file in style/ ). You can copy, rename and adapt it to make your own style
STYLE = 'classic'

# ======== ENABLED FORMATS ===========

#Here you can specify an extra formats module
CUSTOM_FORMATS_MODULE = None

# ======== PREINSTALLED DATA ===========

#INPUTSOURCES = [
#    InputSource(id='sampledocs',label='Sample texts',path=ROOT+'/inputsources/sampledata',defaultmetadata=PlainTextFormat(None, encoding='utf-8') ),
#]

# ======== PROFILE DEFINITIONS ===========

PROFILES = [
    Profile(
        InputTemplate('textinput', PlainTextFormat,"Text Input",
            StaticParameter(id='encoding',name='Encoding',description='The character encoding of the file', value='utf-8'),
            #ChoiceParameter(id='language',name='Language',description='The language the text is in', choices=[('en','English'),('nl','Dutch'),('fr','French')]),
            #StringParameter(id='author',name='Author',description="The author's name", maxlength=100),
            #InputSource(id='sampledoc', label="Sample Document", path=ROOT+'/inputsources/sampledoc.txt', metadata=PlainTextFormat(None, encoding='utf-8',language='en')),
            #CharEncodingConverter(id='latin1',label='Convert from Latin-1',charset='iso-8859-1'),
            #PDFtoTextConverter(id='pdfconv',label='Convert from PDF Document'),
            #MSWordConverter(id='docconv',label='Convert from MS Word Document'),
            extension='.txt',
            #filename='filename.txt',
            #unique=True #set unique=True if the user may only upload a file for this input template once. Set multi=True if you the user may upload multiple of such files
            acceptarchive=True,
            multi=True
        ),
        #------------------------------------------------------------------------------------------------------------------------
        OutputTemplate('foliaout',FoLiAXMLFormat,'Output analysis',
            XSLTViewer(file=TSCANDIR + '/view/tscanview.xsl'),
            removeextension='.txt', #remove prior to adding
            extension='.xml',
            multi=True
        ),
        OutputTemplate('xsl',XMLStyleSheet,'Stylesheet for Visualisation',
            filename='tscanview.xsl',
            unique=True
        ),
        OutputTemplate('wordcsv', CSVFormat, 'Document statistics, per word',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            removeextension='.txt', #remove prior to adding
            extension='csv',
            multi=True
        ),
        OutputTemplate('sencsv', CSVFormat, 'Document statistics, per sentence',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            removeextension='.txt', #remove prior to adding
            extension='csv',
            multi=True
        ),
        OutputTemplate('parcsv', CSVFormat, 'Document statistics, per paragraph',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            removeextension='.txt', #remove prior to adding
            extension='csv',
            multi=True
        ),
        OutputTemplate('doccsv', CSVFormat, 'Document statistics, entire document',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            removeextension='.txt', #remove prior to adding
            extension='csv',
            multi=True
        ),
        OutputTemplate('totalwordcsv', CSVFormat, 'Aggregated statistics, per word',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            filename='total.word.csv',
            unique=True
        ),
        OutputTemplate('totalsencsv', CSVFormat, 'Aggregated statistics, per sentence',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            filename='total.sen.csv',
            unique=True
        ),
        OutputTemplate('totalparcsv', CSVFormat, 'Aggregated statistics, per paragraph',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            filename='total.par.csv',
            unique=True
        ),
        OutputTemplate('totaldoccsv', CSVFormat, 'Aggregated statistics, per document',
            SimpleTableViewer(delimiter=",",quotechar='"'),
            filename='total.doc.csv',
            unique=True
        )
    )
]

# ======== COMMAND ===========

#The system command. It is recommended you set this to small wrapper
#script around your actual system. Full shell syntax is supported. Using
#absolute paths is preferred. The current working directory will be
#set to the project directory.
#
#You can make use of the following special variables,
#which will be automatically set by CLAM:
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
COMMAND = TSCANDIR + "/webservice/tscanwrapper.py $DATAFILE $STATUSFILE $INPUTDIRECTORY $OUTPUTDIRECTORY " + TSCANDIR

# ======== PARAMETER DEFINITIONS ===========

#The parameters are subdivided into several groups. In the form of a list of (groupname, parameters) tuples. The parameters are a list of instances from common/parameters.py

wordfreqlist = []
for f in glob.glob(TSCANDIR + "/data/*words.freq"):
    wordfreqlist.append( (os.path.basename(f), os.path.basename(f)) )

lemmafreqlist = []
for f in glob.glob(TSCANDIR + "/data/*lemma.freq"):
    lemmafreqlist.append( (os.path.basename(f), os.path.basename(f)) )


topfreqlist = []
for f in glob.glob(TSCANDIR + "/data/*20000.freq"):
    topfreqlist.append( (os.path.basename(f), os.path.basename(f)) )


PARAMETERS =  [
    ('Parameters', [
        IntegerParameter(id='rarityLevel',name='Rarity Level',description='Rarity level',default=4),
        IntegerParameter(id='overlapSize',name='Overlap Size',description='Overlap Size',default=50),
        FloatParameter(id='polarity_threshold',name='Polarity Threshold',description='Polarity Threshold',default=0.01),
        #BooleanParameter(id='createlexicon',name='Create Lexicon',description='Generate a separate overall lexicon?'),
        #ChoiceParameter(id='casesensitive',name='Case Sensitivity',description='Enable case sensitive behaviour?', choices=['yes','no'],default='no'),
        #StringParameter(id='author',name='Author',description='Sign output metadata with the specified author name',maxlength=255),
        ChoiceParameter(id='usealpino',name='Use Alpino parser',description='Use Alpino parser?', choices=['yes','no'],default='yes'),
        ChoiceParameter(id='usewopr',name='Use Wopr',description='Use Wopr?', choices=['yes','no'],default='yes'),
        ChoiceParameter(id='word_freq_lex', name='Word Frequency List', description="Word frequency list", choices=wordfreqlist, default="SoNaR500.wordfreqlist_words.freq"),
        ChoiceParameter(id='lemma_freq_lex', name='Lemma Frequency List', description="Lemma frequency list", choices=lemmafreqlist, default="SoNaR500.wordfreqlist_lemma.freq"),
        ChoiceParameter(id='top_freq_lex', name='Top Frequency List', description="Top frequency list", choices=topfreqlist, default="SoNaR500.wordfreqlist20000.freq"),
    ] )
]



# ======== DISPATCHING (ADVANCED! YOU CAN SAFELY SKIP THIS!) ========

#The dispatcher to use (defaults to clamdispatcher.py), you almost never want to change this
#DISPATCHER = 'clamdispatcher.py'

#DISPATCHER_POLLINTERVAL = 30   #interval at which the dispatcher polls for resource consumption (default: 30 secs)
#DISPATCHER_MAXRESMEM = 0    #maximum consumption of resident memory (in megabytes), processes that exceed this will be automatically aborted. (0 = unlimited, default)
#DISPATCHER_MAXTIME = 0      #maximum number of seconds a process may run, it will be aborted if this duration is exceeded.   (0=unlimited, default)
#DISPATCHER_PYTHONPATH = []        #list of extra directories to add to the python path prior to launch of dispatcher

#Run background process on a remote host? Then set the following (leave the lambda in):
#REMOTEHOST = lambda: return 'some.remote.host'
#REMOTEUSER = 'username'

#For this to work, the user under which CLAM runs must have (passwordless) ssh access (use ssh keys) to the remote host using the specified username (ssh REMOTEUSER@REMOTEHOST)
#Moreover, both systems must have access to the same filesystem (ROOT) under the same mountpoint.
