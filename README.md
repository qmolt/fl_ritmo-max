
# About

A real-time external for the Windows version of Max 8 (Max/MSP). fl_ritmo is an external that allows you to create easily any sequence of rhythms using a specific list format of multiple pairs of beats and sequences of notes played. After sending the beat period that will be used, the external will translate a formatted list to a sequence that can be played after a trigger signal is sent. An example of the format used:

``bar 2. <1111 3.5 <001`` means that the external will play 4 notes in a 4th division of 2.0 beats and the last note in a 3rd division of 3.5 beats. 
This allows you to make very complex rhythms if you want to.

The output will be the subdivision duration in milliseconds of its respective subdivision as a float.

This external has the same functionality as fl_batuta when adding multiple notes to a bar. 