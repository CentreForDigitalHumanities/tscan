#!/usr/bin/env python
#-*- coding:utf-8 -*-

###############################################################
# -- Tscan Wrapper script for CLAM --
#       by Maarten van Gompel (proycon)
#       http://ilk.uvt.nl/~mvgompel
#       Induction for Linguistic Knowledge Research Group
#       Universiteit van Tilburg
#
#       Licensed under GPLv3
#
###############################################################


#This script will be called by CLAM and will run with the current working directory set to the specified project directory

#import some general python modules:
import sys
import os
import shutil
import glob

#import CLAM-specific modules. The CLAM API makes a lot of stuff easily accessible.
import clam.common.data
import clam.common.status



#this script takes three arguments from CLAM: $DATAFILE $STATUSFILE $OUTPUTDIRECTORY  (as configured at COMMAND= in the service configuration file)
datafile = sys.argv[1]
statusfile = sys.argv[2]
inputdir = sys.argv[3]
outputdir = sys.argv[4]
TSCANDIR = sys.argv[5]

#Obtain all data from the CLAM system (passed in $DATAFILE (clam.xml))
clamdata = clam.common.data.getclamdata(datafile)

#You now have access to all data. A few properties at your disposition now are:
# clamdata.system_id , clamdata.project, clamdata.user, clamdata.status , clamdata.parameters, clamdata.inputformats, clamdata.outputformats , clamdata.input , clamdata.output

clam.common.status.write(statusfile, "Starting...")

if not 'word_freq_lex' in clamdata:
    print >>sys.stderr, "Missing parameter: word_freq_lex"
    sys.exit(2)
if not 'lemma_freq_lex' in clamdata:
    print >>sys.stderr, "Missing parameter: lemma_freq_lex"
    sys.exit(2)
if not 'top_freq_lex' in clamdata:
    print >>sys.stderr, "Missing parameter: top_freq_lex"
    sys.exit(2)



#Write configuration file

f = open(outputdir + '/tscan.cfg','w')
if 'usealpino' in clamdata and clamdata['usealpino'] == 'yes':
    f.write("useAlpinoServer=1\n")
    f.write("useAlpino=1\n")
else:
    f.write("useAlpinoServer=0\n")
    f.write("useAlpino=0\n")
if 'usewopr' in clamdata and clamdata['usewopr'] == 'yes':
    f.write("useWopr=1\n")
else:
    f.write("useWopr=0\n")
f.write("decompounderPath=\"" + TSCANDIR + "\"\n")
f.write("surprisalPath=\"" + TSCANDIR + "\"\n")
f.write("styleSheet=\"tscanview.xsl\"\n")

if 'rarityLevel' in clamdata:
	raritylevel = clamdata['rarityLevel']
else:
	raritylevel = 4
if 'overlapSize' in clamdata:
	overlapsize = clamdata['overlapSize']
else:
	overlapsize = 50
if 'polarity_threshold' in clamdata:
	polarity = clamdata['polarity_threshold']
else:
	polarity = 0.01
f.write("rarityLevel="  + str(raritylevel) + "\n")
f.write("overlapSize="  + str(overlapsize) + "\n")
f.write("polarity_threshold="  + str(polarity) + "\n")

f.write("configDir="+ TSCANDIR + "/data\n")
f.write("adj_semtypes=\"adjs_semtype.data\"\n")
f.write("verb_semtypes=\"verbs_semtype.data\"\n")
f.write("noun_semtypes=\"nouns_semtype.data\"\n")
f.write("polarity_lex=\"polarity_lexicon.data\"\n")
#f.write("freq_lex=\"dcoi_staphformat.freq\"\n")

f.write("word_freq_lex=\"" + clamdata['word_freq_lex'] + "\"\n")  #freqlist_staphorsius_CLIB_words.freq
f.write("lemma_freq_lex=\"" + clamdata['lemma_freq_lex'] + "\"\n") #freqlist_staphorsius_CLIB_lemma.freq
f.write("staph_word_freq_lex=\"freqlist_staphorsius_CLIB_words.freq\"\n")  #freqlist_staphorsius_CLIB_words.freq
f.write("top_freq_lex=\"" + clamdata['top_freq_lex'] + "\"\n")  #freqlist_staphorsius_CLIB_words.freq

f.write("voorzetselexpr=\"voorzetseluitdrukkingen.txt\"\n")
f.write("temporals=\"temporal_connectors.lst\"\n")
f.write("opsommers=\"opsom_connectors.lst\"\n")
f.write("contrast=\"contrast_connectors.lst\"\n")
f.write("compars=\"compar_connectors.lst\"\n")
f.write("causals=\"causal_connectors.lst\"\n")


f.write("[[frog]]\n") #Frog server should already be runnning, start manually
f.write("port=7345\n")
f.write("host=localhost\n\n")


f.write("[[wopr]]\n")
f.write("port=6345\n")
f.write("host=localhost\n\n")

f.write("[[alpino]]\n")
f.write("port=6666\n")
f.write("host=localhost\n")


f.close()

os.system('svn info ' + TSCANDIR + ' >&2')


#-- Iterate over all input files? --

for inputfile in clamdata.inputfiles('textinput'):
   if '"' in str(inputfile):
       clam.common.status.write(statusfile, "Failed, filename has a &quot;, illegal!",100) # status update
       sys.exit(2)
   inputtemplate = inputfile.metadata.inputtemplate
   clam.common.status.write(statusfile, "Processing " + os.path.basename(str(inputfile)),50) # status update
   ref = os.system('ALPINO_HOME="/vol/customopt/alpino" tscan --config=' + outputdir + '/tscan.cfg -t \"' + str(inputfile) + '\" -o \"' + outputdir + '/' + os.path.basename(str(inputfile).replace('.txt','') + '.xml') + '"')
   if ref != 0:
        clam.common.status.write(statusfile, "Failed",50) # status update

#tscan writes CSV file in input directory, move:
os.system("mv -f " + inputdir + "/*.csv " + outputdir)

os.system("cat " + outputdir + "/*.words.csv | head -n 1 > " + outputdir + "/total.word.csv")
os.system("cat " + outputdir + "/*.paragraphs.csv | head -n 1 > " + outputdir + "/total.par.csv")
os.system("cat " + outputdir + "/*.sentences.csv | head -n 1 > " + outputdir + "/total.sen.csv")

for f in glob.glob(outputdir + "/*.words.csv"): os.system("sed 1d " + f + " >> " + outputdir + "/total.word.csv")
for f in glob.glob(outputdir + "/*.paragraphs.csv"): os.system("sed 1d " + f + " >> " + outputdir + "/total.par.csv")
for f in glob.glob(outputdir + "/*.sentences.csv"): os.system("sed 1d " + f + " >> " + outputdir + "/total.sen.csv")
for f in glob.glob(outputdir + "/*.document.csv"): os.system("sed 1d " + f + " >> " + outputdir + "/total.doc.csv")


shutil.copyfile(TSCANDIR + "/view/tscanview.xsl", outputdir + "/tscanview.xsl")

#A nice status message to indicate we're done
clam.common.status.write(statusfile, "Done",100) # status update

sys.exit(0) #non-zero exit codes indicate an error and will be picked up by CLAM as such!
