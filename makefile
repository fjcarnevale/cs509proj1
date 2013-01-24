all: play record

play: play.c
	gcc play.c -o play -lasound

record: record.c
	gcc record.c -o record -lasound

speaker_test: speaker_test.c
	gcc speaker_test.c -o speaker_test -lasound

