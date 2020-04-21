TRANSLATE
=========

Homework for course "Highload web-systems"

Service that has some dictionaries and can translate word from one language to another.

Web service with REST-API:

* GET /translate/ -- show list of supported languages.

* GET /translate/LANG/ -- show list of known words for the choosen language.

* GET /translate/LANG/WORD/ -- show list of languages to translate WORD from LANG

* GET /translate/LANG1/WORD/LANG2/ -- show translation of WORD from LANG1 to LANG2

* POST /update/LANG1/LANG2/ -- update (or add a new) dictionary. The dictionary is in request body with a follow format: "WORD1,WORD2" where WORD1 is word from LANG1 and WORD2 is it's translation.
