#!/usr/bin/python
# -*- coding: utf-8 -*-

import os, numpy, sys
from gensim import corpora, models, similarities, matutils
from twisted.internet import protocol, reactor #No Python 3 support yet :(
from twisted.protocols import basic

# het queryen van zinnen/teksten staat uitgelegd op http://radimrehurek.com/gensim/tut3.html .
# Om een of andere reden werkt het bij mij niet om losse woorden op deze manier met elkaar te vergelijken (documenten van 1 woord).
# Onderstaande methode waarbij "zelf" de similarity scores uitreken levert wel resultaten op die ok zijn.



class LSAProtocol2(basic.LineReceiver):
    def lineReceived(self, line):
        #print( "LSA received " + line );
        try:
            sent1, sent2 = line.strip().split("\t")
        except:
            self.sendLine("INPUTERROR: missing tab character?")
            return

        #print( "LSA sentence 1: " + sent1 )
        #print( "LSA sentence 2: " + sent2 )
        try:
            vec_bow1 = dictionary.doc2bow( sent1.lower().split())
            vec_bow2 = dictionary.doc2bow( sent2.lower().split())
            vec_lsi1 = lsi[vec_bow1]
            vec_lsi2 = lsi[vec_bow2]
        except KeyError:
            self.sendLine(str(0))
            return
        if not vec_lsi1 or not vec_lsi2:
            self.sendLine(str(0))
            return

        #print "LSA vector1 :"
        #print vec_lsi1
        #print "LSA vector2 : "
        #print vec_lsi2
        try:
            cossim = numpy.dot(matutils.unitvec(numpy.array([ x[1] for x in vec_lsi1])),
                               matutils.unitvec(numpy.array([ x[1] for x in vec_lsi2])) )
        except:
            print "dot product faalt"
            cossim = 0
            raise
        self.sendLine(str(cossim))

class LSAFactory2(protocol.ServerFactory):
    def __init__(self, dictionary, lsi):
        self.dictionary = dictionary
        self.lsi = lsi

class LSAServer2(object):
    def __init__(self, dictionary, lsi, port=65430):
        factory = LSAFactory2(dictionary, lsi)
        factory.protocol = LSAProtocol2;
        reactor.listenTCP(port, factory )
        reactor.run()

if __name__ == '__main__':
    try:
        port = int(sys.argv[1])
        dict_file, lsa_file = sys.argv[2:]
    except:
        print >>sys.stderr,"Usage: port dict_file lsa_file"
        sys.exit(2)

    print( "reading the data files. (takes some time...)" );
    #dict_file = 'newspaper_contentlemma_dictionary_filtered.dict'
    #bow_corp_file = 'newspaper_contentlemma_corpus_as_vectors_filtered.mm' //not used?
    #lsa_file = 'lsa_newspaper_lemma_model_700_filtered_tfidf.lsi'

    # Inladen van eerder gecreëerde corpus, dictionary en lsa objecten
    #corpus = corpora.MmCorpus(bow_corp_file) # I don't see the need for this one?
    lsi = models.LsiModel.load(lsa_file)
    dictionary = corpora.Dictionary.load(dict_file)

    print( "starting the LSA2 server!" );
    LSAServer2(dictionary, lsi, port)

