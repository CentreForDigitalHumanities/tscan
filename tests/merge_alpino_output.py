#!/usr/bin/env python3
from lxml import etree
from glob import glob
import re


def get_tree(filename, index):
    corpus = etree.parse(filename)
    root = corpus.getroot()
    if root.tag == 'alpino_ds':
        return root

    trees = corpus.findall("alpino_ds")
    return trees[index-1]


def get_alpino_target_filename(filename):
    testname = re.sub("\.example\..*alpino\.xml", "", filename)
    return testname + ".example.alpino"


def get_sentences():
    lookups = glob("out*.alpino_lookup.data")
    for lookup in lookups:
        with open(lookup, "r") as lookup_file:
            lines = lookup_file.readlines()
            for line in lines:
                sentence, filename, index = line.split("\t")
                yield sentence, get_tree(filename, int(index)), get_alpino_target_filename(filename)


merged_treebanks = {}
sentences = {}
for sentence, tree, alpino_target_filename in get_sentences():
    sentences[sentence] = (tree, alpino_target_filename)

with open("alpino_lookup.data", "w") as target:
    for sentence, (tree, alpino_target_filename) in sentences.items():
        try:
            merged_treebank = merged_treebanks[alpino_target_filename]
        except KeyError:
            merged_treebank = etree.Element("treebank")
            merged_treebanks[alpino_target_filename] = merged_treebank
        merged_treebank.append(tree)
        # numbering of treebanks is 1 based
        target.write(
            f"{sentence}\t{alpino_target_filename}\t{len(merged_treebank.getchildren())}\n")

for merged_filename, merged_treebank in merged_treebanks.items():
    etree.indent(merged_treebank)
    etree.ElementTree(merged_treebank).write(
        merged_filename, xml_declaration=True, pretty_print=True, encoding="UTF-8")
