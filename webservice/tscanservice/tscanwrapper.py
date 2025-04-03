#!/usr/bin/env python3
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
from __future__ import print_function
from itertools import chain

#import some general python modules:
import sys
import os
import shutil
import glob
import signal
import re

from lxml import etree

#import CLAM-specific modules. The CLAM API makes a lot of stuff easily accessible.
import clam.common.data
import clam.common.status


#this script takes three arguments from CLAM: $DATAFILE $STATUSFILE $OUTPUTDIRECTORY  (as configured at COMMAND= in the service configuration file)
datafile = sys.argv[1]
statusfile = sys.argv[2]
inputdir = sys.argv[3]
outputdir = sys.argv[4]
TSCANDIR = sys.argv[5]
TSCANDATA = sys.argv[6]
TSCANSRC = sys.argv[7]
ALPINOHOME = sys.argv[8]


# Remove all the output files from a crashed earlier attempt
for filepath in chain(
    glob.glob(f"{inputdir}/*.tscan.xml"),
    glob.glob(f"{inputdir}/*.csv")):
    os.remove(filepath)

#Obtain all data from the CLAM system (passed in $DATAFILE (clam.xml))
clamdata = clam.common.data.getclamdata(datafile)

#You now have access to all data. A few properties at your disposition now are:
# clamdata.system_id , clamdata.project, clamdata.user, clamdata.status , clamdata.parameters, clamdata.inputformats, clamdata.outputformats , clamdata.input , clamdata.output

clam.common.status.write(statusfile, "Starting...")

if not 'word_freq_lex' in clamdata:
    print("Missing parameter: word_freq_lex", file=sys.stderr)
    sys.exit(2)
if not 'lemma_freq_lex' in clamdata:
    print("Missing parameter: lemma_freq_lex", file=sys.stderr)
    sys.exit(2)
if not 'top_freq_lex' in clamdata:
    print("Missing parameter: top_freq_lex", file=sys.stderr)
    sys.exit(2)


def load_custom_wordlist(configfile, inputdir, tscan_name, inputtemplate, default_location=None):
    """This allows custom word lists. Does require to specify the full path to the files."""
    wordlist = None

    for inputfile in clamdata.inputfiles(inputtemplate):
        wordlist = inputdir + inputfile.filename
        break
    else:
        # When no inputfile is found, revert to the default (if there is one)
        if default_location:
            wordlist = TSCANDATA + default_location

    # Write the wordlist to the config file
    if wordlist:
        configfile.write(tscan_name + "=\"" + wordlist + "\"\n")


def save_alpino_lookup(outputdir, alpino_lookup):
    with open(outputdir + '/alpino_lookup.data', 'w') as alpino_lookup_file:
        for sentence, filepath, index in alpino_lookup:
            alpino_lookup_file.write(f"{sentence}\t{filepath}\t{index}\n")


def get_tree(filename, index):
    corpus = etree.parse(filename)
    root = corpus.getroot()
    if root.tag == 'alpino_ds':
        return root

    trees = corpus.findall("alpino_ds")
    # index is 1-based
    return trees[index-1]


def get_output_trees(inputdir, outputdir):
    with open(outputdir + '/alpino_lookup.data', 'r') as alpino_lookup_file:
        for line in alpino_lookup_file.readlines():
            sentence, filename, index = line.split("\t")
            if filename.startswith("input/"):
                filename = filename.replace("input/", inputdir)
            yield sentence, get_tree(filename, int(index))


def init_alpino_lookup(configfile, inputdir, outputdir):
    alpino_lookup = []

    # hidden files containing the output of the parsing are added to
    # the input folder, to speed up (re)analyzing the same sentences
    alpino_filenames = set(
        chain(
            (alpino_xml.filename for alpino_xml in clamdata.inputfiles("alpino")),
            (os.path.basename(x) for x in glob.iglob(inputdir + ".*.alpino.xml"))))

    for alpino_filename in alpino_filenames:
        filepath = inputdir + alpino_filename
        corpus = etree.parse(filepath)
        root = corpus.getroot()
        # treebanks start counting from zero, xpath queries are also 1-based
        trees = [(0, root)] if root.tag == 'alpino_ds' else enumerate(
            corpus.findall("alpino_ds"), 1)
        for index, tree in trees:
            # reconstruct the sentence from the words
            words = {}
            for word in tree.findall(".//node[@word]"):
                words[int(word.attrib['begin'])] = word.attrib['word']
            sentence = ' '.join(
                map(lambda x: x[1], sorted(words.items(), key=lambda x: x[0])))
            alpino_lookup.append((sentence, filepath, index))

    # always save the Alpino lookup even when empty:
    # this way when we're dealing with multiple documents,
    # there will always be an Alpino lookup which can processed
    save_alpino_lookup(outputdir, alpino_lookup)

    configfile.write(f"alpino_lookup=\"{outputdir}/alpino_lookup.data\"\n")

#Write configuration file

f = open(outputdir + '/tscan.cfg', 'w')
if 'useAlpino' in clamdata and clamdata['useAlpino'] == 'yes':
    f.write("useAlpinoServer=1\n")
    f.write("useAlpino=1\n")
    f.write("saveAlpinoOutput=1\n")
    f.write("saveAlpinoMetadata=0\n")
else:
    f.write("useAlpinoServer=0\n")
    f.write("useAlpino=0\n")
if 'useWopr' in clamdata and clamdata['useWopr'] == 'yes':
    f.write("useWopr=1\n")
else:
    f.write("useWopr=0\n")
if 'compoundSplitterMethod' in clamdata and clamdata['compoundSplitterMethod'] != 'none':
    f.write("useCompoundSplitter=1\n")
else:
    f.write("useCompoundSplitter=0\n")

if 'sentencePerLine' in clamdata and clamdata['sentencePerLine'] == 'yes':
    f.write("sentencePerLine=1\n")
else:
    f.write("sentencePerLine=0\n")

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

if 'frequencyClip' in clamdata:
    freqclip = clamdata['frequencyClip']
else:
    freqclip = 99

if 'mtldThreshold' in clamdata:
    mtldthreshold = clamdata['mtldThreshold']
else:
    mtldthreshold = 0.720

f.write("rarityLevel=" + str(raritylevel) + "\n")
f.write("overlapSize=" + str(overlapsize) + "\n")
f.write("frequencyClip=" + str(freqclip) + "\n")
f.write("mtldThreshold=" + str(mtldthreshold) + "\n")

f.write("configDir=" + TSCANDATA + "\n")
f.write("verb_semtypes=\"verbs_semtype.data\"\n")
f.write("general_nouns=\"general_nouns.data\"\n")
f.write("general_verbs=\"general_verbs.data\"\n")
f.write("adverbs=\"adverbs.data\"\n")

init_alpino_lookup(f, inputdir, outputdir)

# 20180305: This allows a stoplist.
load_custom_wordlist(f, inputdir, "stop_lemmata", "stoplist")
# 20160802: This allows a completely dcustom classification.
load_custom_wordlist(f, inputdir, "my_classification", "myclassification")
# 20150316: This allows custom adjective classification.
load_custom_wordlist(f, inputdir, "adj_semtypes", "adjclassification", "/adjs_semtype.data")
# 20141121: This allows a custom noun classification.
load_custom_wordlist(f, inputdir, "noun_semtypes", "nounclassification", "/nouns_semtype.data")
# 20150203: This allows custom intensifying words.
load_custom_wordlist(f, inputdir, "intensify", "intensify", "/intensiveringen.data")

f.write("word_freq_lex=\"" + clamdata['word_freq_lex'] + "\"\n")  # freqlist_staphorsius_CLIB_words.freq
f.write("lemma_freq_lex=\"" + clamdata['lemma_freq_lex'] + "\"\n")  # freqlist_staphorsius_CLIB_lemma.freq
f.write("staph_word_freq_lex=\"freqlist_staphorsius_CLIB_words.freq\"\n")  # freqlist_staphorsius_CLIB_words.freq
f.write("top_freq_lex=\"" + clamdata['top_freq_lex'] + "\"\n")  # freqlist_staphorsius_CLIB_words.freq

f.write("voorzetselexpr=\"voorzetseluitdrukkingen.txt\"\n")
f.write("afkortingen=\"afkortingen.lst\"\n")
f.write("temporals=\"temporal_connectors.lst\"\n")
f.write("opsom_connectors_wg=\"opsom_connectors_wg.lst\"\n")
f.write("opsom_connectors_zin=\"opsom_connectors_zin.lst\"\n")
f.write("contrast=\"contrast_connectors.lst\"\n")
f.write("compars=\"compar_connectors.lst\"\n")
f.write("causals=\"causal_connectors.lst\"\n")
f.write("causal_situation=\"causaliteit.txt\"\n")
f.write("space_situation=\"ruimte.txt\"\n")
f.write("time_situation=\"tijd.txt\"\n")
f.write("emotion_situation=\"emoties.txt\"\n")

prevalence = 'nl'
if 'prevalence' in clamdata:
    prevalence = clamdata['prevalence']
f.write("prevalence=\"prevalence_" + prevalence + ".data\"\n")
f.write("formal=\"formal.data\"\n")


f.write("[[frog]]\n")  # Frog server should already be runnning, start manually
f.write("port=7001\n")
f.write("host=127.0.0.1\n\n")

f.write("[[wopr]]\n")
f.write("port_fwd=7020\n")
f.write("host_fwd=127.0.0.1\n\n")
f.write("port_bwd=7002\n")
f.write("host_bwd=127.0.0.1\n\n")

f.write("[[alpino]]\n")
f.write("port=7003\n")
f.write("host=127.0.0.1\n")

if 'compoundSplitterMethod' in clamdata and clamdata['compoundSplitterMethod'] != 'none':
    f.write("\n[[compound_splitter]]\n")
    f.write("port=7005\n")
    f.write("host=127.0.0.1\n")
    method = clamdata['compoundSplitterMethod']
    f.write('method="{}"\n'.format(method))

f.close()

#collect all input files
inputfiles = []
for inputfile in clamdata.inputfiles('textinput'):
    if '"' in str(inputfile):
        clam.common.status.write(statusfile, "Failed, filename has a &quot;, illegal!", 100)  # status update
        sys.exit(2)
    inputtemplate = inputfile.metadata.inputtemplate
    inputfiles.append(str(inputfile))


def copy_modify_metadata(outputdir: str, source_filename: str, total_filename: str) -> None:
    with open(os.path.join(outputdir, f".{source_filename}.METADATA"), 'r') as source_file:
        with open(os.path.join(outputdir, f".{total_filename}.METADATA"), 'w') as target_file:
            content = source_file.read()
            content = content.replace("Document statistics", "Corpus statistics")
            content = content.replace(re.sub(r'\.[^\.]+\.csv$', '', source_filename), "TOTAL")
            target_file.write(content)


def merge_output(outputdir: str, type: str) -> None:
    first_file = True
    total_filename = f"total.{type}.csv"

    # create the empty target file
    open(total_filename, 'w').close()

    for f in glob.glob(f"{outputdir}/*.{type}.csv"):
        filename = os.path.basename(f)
        if filename == total_filename:
            # ignore lingering total files
            continue

        if first_file:
            # copy and modify metadata
            copy_modify_metadata(outputdir, filename, total_filename)

        first_line = True
        with open(os.path.join(outputdir, total_filename), 'a') as target_file:
            with open(os.path.join(outputdir, filename), 'r') as source_file:
                while True:
                    line = source_file.readline()
                    if not line:
                        break

                    if first_line and not first_file:
                        # skip the first line
                        first_line = False
                    else:
                        # except for the first file, so the file will have the column names
                        first_file = False
                        target_file.write(line)


def sigterm_handler():
    #collect output
    clam.common.status.write(statusfile, "Postprocessing after forceful abortion", 90)  # status update
    for inputfile in inputfiles:
        os.rename(inputfile + '.tscan.xml', outputdir + '/' + os.path.basename(inputfile).replace('.txt.tscan', '').replace('.txt', '') + '.xml')

    #tscan writes CSV file in input directory, move:
    os.system("mv -f " + inputdir + "/*.csv " + outputdir)

    merge_output(outputdir, "words")
    merge_output(outputdir, "paragraphs")
    merge_output(outputdir, "sentences")
    merge_output(outputdir, "document")
    sys.exit(5)

signal.signal(signal.SIGTERM, sigterm_handler)

shutil.copyfile(TSCANSRC + "/view/tscanview.xsl", outputdir + "/tscanview.xsl")

# remove any previous output
for f in glob.glob(outputdir + "/../out*.alpino_lookup.data"):
    os.remove(f)

#pass all input files at once
clam.common.status.write(statusfile, "Processing " + str(len(inputfiles)) + " files, this may take a while...", 10)  # status update
ref = 0
step_size = 80 / len(inputfiles)
for i, infile in enumerate(inputfiles):
    if i > 0:
        try:
            # Use the generated lookup file as input
            os.rename(outputdir + "/../out.alpino_lookup.data", outputdir + "/alpino_lookup.data")
            clam.common.status.write(statusfile, "Updated Alpino lookup cache", int(10 + i * step_size))
        except FileNotFoundError:
            clam.common.status.write(statusfile, "No Alpino parses, empty document?", int(10 + i * step_size))
    clam.common.status.write(statusfile, f"Started processing ... {infile}", int(10 + i * step_size))
    try:
        exit_status = os.system('ALPINO_HOME="' + ALPINOHOME + '" TCL_LIBRARY="' + ALPINOHOME + '/create_bin/tcl8.5" TCLLIBPATH="' + ALPINOHOME + '/create_bin/tcl8.5" tscan --config=' + outputdir + '/tscan.cfg ' + infile)
    except Exception as error:
        exit_status = 1

    if exit_status == 0:
        clam.common.status.write(statusfile, f"Finished processing {infile}", int(10 + i * step_size))
    else:
        clam.common.status.write(statusfile, f"PROBLEM PROCESSING {infile} ERROR CODE {exit_status} consult error log", int(10 + i * step_size))
        ref = exit_status


#collect output
clam.common.status.write(statusfile, "Postprocessing", 90)  # status update
for inputfile in inputfiles:
    try:
        os.rename(inputfile + '.tscan.xml', outputdir + '/' + os.path.basename(inputfile).replace('.txt.tscan', '').replace('.txt', '') + '.xml')
    except FileNotFoundError:
        print("Expected output file " + inputfile + ".tscan.xml not created, something went wrong earlier?", file=sys.stderr)

if ref != 0:
    clam.common.status.write(statusfile, "Failed", 90)  # status update

#tscan writes CSV file in input directory, move:
os.system("mv -f " + inputdir + "/*.csv " + outputdir)

#write the csv headers to a file with all results ('total.<type>.csv')
merge_output(outputdir, "words")
merge_output(outputdir, "paragraphs")
merge_output(outputdir, "sentences")
merge_output(outputdir, "document")

# Merge all the Alpino output
if 'alpinoOutput' in clamdata and clamdata['alpinoOutput'] != 'no':
    clam.common.status.write(statusfile, "Merging Alpino output", 95)
    # Move the generated lookup file
    os.rename(outputdir + "/../out.alpino_lookup.data", outputdir + "/alpino_lookup.data")
    merged_lookup = []
    merged_treebank = etree.Element("treebank")
    for sentence, tree in get_output_trees(inputdir, outputdir):
        merged_treebank.append(tree)
        merged_lookup.append(
            (sentence, "alpino.xml", len(merged_treebank.getchildren())))

    etree.indent(merged_treebank)
    etree.ElementTree(merged_treebank).write(
        outputdir + "/alpino.xml", xml_declaration=True, pretty_print=True, encoding="UTF-8")
    save_alpino_lookup(outputdir, merged_lookup)

# A nice status message to indicate we're done
clam.common.status.write(statusfile, "Done", 100)  # status update

# non-zero exit codes indicate an error and will be picked up by CLAM as such!
sys.exit(ref)
