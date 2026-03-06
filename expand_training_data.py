#!/usr/bin/env python3
"""
Expand training data with more diverse content
Creates a larger corpus from multiple topics
"""

topics = {
    'technology': [
        'Artificial intelligence is the simulation of human intelligence by machines.',
        'Machine learning enables computers to learn from data without explicit programming.',
        'Deep learning uses neural networks with multiple layers to process information.',
        'Natural language processing helps computers understand and generate human language.',
        'Computer vision allows machines to interpret and analyze visual information.',
        'Neural networks are computational models inspired by biological brain structures.',
        'Big data refers to extremely large datasets that require special processing techniques.',
        'Cloud computing provides on-demand access to computing resources over the internet.',
        'Blockchain is a distributed ledger technology that ensures secure transactions.',
        'Quantum computing uses quantum mechanical phenomena to perform calculations.',
    ],
    'science': [
        'Physics is the natural science that studies matter, energy, and their interactions.',
        'Chemistry examines the properties, composition, and behavior of matter.',
        'Biology is the study of living organisms and their vital processes.',
        'Astronomy explores celestial objects, space, and the universe as a whole.',
        'Genetics studies genes, genetic variation, and heredity in organisms.',
        'Evolution is the process by which species change over successive generations.',
        'DNA contains the genetic instructions used in the development of all living things.',
        'Photosynthesis is the process by which plants convert light energy into chemical energy.',
        'The scientific method is a systematic approach to investigating natural phenomena.',
        'Molecular biology studies the structure and function of biological molecules.',
    ],
    'mathematics': [
        'Mathematics is the study of numbers, quantities, shapes, and patterns.',
        'Algebra uses symbols and letters to represent numbers in equations.',
        'Calculus deals with rates of change and the accumulation of quantities.',
        'Geometry studies the properties and relationships of points, lines, and shapes.',
        'Statistics involves collecting, analyzing, and interpreting numerical data.',
        'Probability measures the likelihood of events occurring in random experiments.',
        'Number theory investigates the properties and relationships of integers.',
        'Topology examines properties of space that are preserved under continuous deformations.',
        'Linear algebra studies vector spaces and linear mappings between them.',
        'Discrete mathematics deals with countable, distinct mathematical structures.',
    ],
    'history': [
        'History is the study of past events and human civilization.',
        'The Renaissance was a period of cultural rebirth in Europe from the 14th to 17th century.',
        'The Industrial Revolution transformed manufacturing and society beginning in the 18th century.',
        'World War I was a global conflict that lasted from 1914 to 1918.',
        'World War II was fought between 1939 and 1945 involving most nations.',
        'The Cold War was a period of geopolitical tension between the United States and Soviet Union.',
        'Ancient civilizations developed writing, agriculture, and complex social structures.',
        'The printing press revolutionized communication and knowledge dissemination in the 15th century.',
        'Democracy originated in ancient Greece and has evolved over millennia.',
        'The Space Age began with the launch of Sputnik in 1957.',
    ],
    'geography': [
        'Geography is the study of Earth, its features, inhabitants, and phenomena.',
        'Mountains are large landforms that rise prominently above their surroundings.',
        'Oceans cover more than 70 percent of the Earth surface.',
        'Rivers are natural watercourses that flow toward an ocean, sea, or lake.',
        'Climate is the long-term pattern of weather in a particular area.',
        'Continents are large continuous masses of land separated by oceans.',
        'Ecosystems consist of living organisms interacting with their physical environment.',
        'The atmosphere is the layer of gases surrounding Earth.',
        'Plate tectonics explains the movement of Earth lithospheric plates.',
        'The water cycle describes the continuous movement of water on Earth.',
    ],
    'philosophy': [
        'Philosophy is the study of fundamental questions about existence, knowledge, and ethics.',
        'Ethics examines questions of morality, right and wrong, good and bad.',
        'Logic is the systematic study of valid reasoning and argumentation.',
        'Metaphysics investigates the fundamental nature of reality and existence.',
        'Epistemology is the study of knowledge and justified belief.',
        'Political philosophy examines concepts like justice, rights, and governance.',
        'Aesthetics explores the nature of beauty, art, and taste.',
        'Existentialism emphasizes individual existence, freedom, and choice.',
        'Rationalism holds that reason is the primary source of knowledge.',
        'Empiricism asserts that knowledge comes primarily from sensory experience.',
    ],
    'language': [
        'Language is a system of communication using words, sounds, and grammar.',
        'Linguistics is the scientific study of language and its structure.',
        'Grammar defines the rules for forming correct sentences in a language.',
        'Syntax examines how words combine to form phrases and sentences.',
        'Semantics studies the meaning of words, phrases, and sentences.',
        'Phonetics investigates the sounds of human speech.',
        'Morphology analyzes the structure and formation of words.',
        'Translation converts text from one language to another while preserving meaning.',
        'Bilingualism is the ability to speak two languages fluently.',
        'Etymology traces the historical origin and development of words.',
    ],
    'economics': [
        'Economics studies how societies allocate scarce resources to satisfy unlimited wants.',
        'Supply and demand determine prices in a market economy.',
        'Inflation is the rate at which the general level of prices increases over time.',
        'GDP measures the total value of goods and services produced in a country.',
        'Microeconomics focuses on individual consumers and firms in markets.',
        'Macroeconomics examines the economy as a whole including inflation and unemployment.',
        'Trade allows countries to specialize and exchange goods and services.',
        'Competition drives efficiency and innovation in market economies.',
        'Monetary policy involves managing money supply and interest rates.',
        'Fiscal policy uses government spending and taxation to influence the economy.',
    ],
}

# Generate expanded corpus
output_file = 'wiki_training_expanded.txt'
lines_written = 0

with open(output_file, 'w', encoding='utf-8') as f:
    # Write each topic multiple times with variations
    for repeat in range(20):  # 20 repetitions
        for topic, sentences in topics.items():
            for sentence in sentences:
                # Original sentence
                f.write(sentence + '\n')
                lines_written += 1

                # Variation 1: "This means that..."
                f.write(f'This means that {sentence[0].lower()}{sentence[1:]}\n')
                lines_written += 1

                # Variation 2: "In other words..."
                f.write(f'In other words, {sentence[0].lower()}{sentence[1:]}\n')
                lines_written += 1

                # Variation 3: Question form
                if sentence.endswith('.'):
                    base = sentence[:-1]
                    f.write(f'What is {base[0].lower()}{base[1:]}?\n')
                    lines_written += 1

                # Variation 4: "We know that..."
                f.write(f'We know that {sentence[0].lower()}{sentence[1:]}\n')
                lines_written += 1

print(f'Created {output_file}')
print(f'Total lines: {lines_written:,}')
print(f'Topics: {len(topics)}')
print(f'Repetitions: 20')
print('')
print('Ready for training!')
