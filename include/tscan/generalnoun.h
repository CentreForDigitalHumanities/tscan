#ifndef GENERALNOUN_H
#define	GENERALNOUN_H

namespace GeneralNoun {

    enum Type {
        ADDITION_ALTERNATIVE,
        IMPORTANCE_INTEREST,
        DESCRIPTION,
        EXISTENCE,
        PHRASING,
        CONCEPT_SYSTEM,
        CONTRAST_VARIATION,
        DISCUSSION,
        REACHING_GOALS,
        FACTUAL_CORRECTNESS,
        EVENT,
        THOUGHT_POSITION,
        GRADATION,
        ACTS_CHOICES,
        INFORMATION,
        INTERPRETATION,
        KNOWLEDGE,
        MEANS_GOAL,
        OPPORTUNITY,
        DEVELOPMENT_STABILITY,
        PROBLEM_SOLUTION,
        REASONING_CAUSALITY,
        STATE,
        STRUCTURE,
        DESIRABILITY,
        NO_GENERAL_NOUN
    };
    
    std::string toString(Type);
    Type classify(const std::string&);
    bool isSeparate(const Type);
    bool isRelated(const Type);
    bool isActing(const Type);
    bool isKnowledge(const Type);
    bool isDiscussion(const Type);
    bool isDevelopment(const Type);
    std::ostream& operator<<( std::ostream&, const Type&);
}

#endif /* GENERALNOUN_H */
