from setuptools import setup, find_packages

with open('README.md') as file:
    long_description = file.read()

setup(
    name='tscan',
    version='0.9.9',
    packages=find_packages(include=['libtscan', 'libtscan.*']),
    package_data={'libtscan': ['py.typed']},
    data_files=[('', ['README.md'])],
    description='An analysis tool for Dutch texts to assess the complexity of the text, based on original work by Rogier Kraf.',
    long_description=long_description,
    long_description_content_type="text/markdown",
    license='AGPL-3.0',
    author='Centre for Digital Humanities, Utrecht University; Tilburg centre for Cognition and Communication, Tilburg University; Language Machines, Centre for Language Studies, Nijmegen',
    author_email='digitalhumanities@uu.nl',
    url='https://github.com/UUDigitalHumanitieslab/tscan',
    install_requires=[
        'folia',
        'python-frog'
    ],
    python_requires='>=3.8',
    zip_safe=True
)
