# ffmpeg-cs3505

Created for Dr. Peter Jenson's 2020 Spring CS 3505 ("Software Practices II") class. Project contians a codec for an "asif" file format through the implementation of a muxer/demuxer and encoder/decoder.

# "asif" file format

Byte Offset	Field	Meaning
- +0	Four characters "asif"	To identify the data as "asif" data
- +4	32-bit little endian int	Sample rate (frequency)
- +8	16-bit little endian int	Number of channels
- +10	32-bit little endian int	Number of samples per channel (n)
- +14	8-bit unsigned data samples for channel 0	sample and (n-1) deltas (in order) for channel 0.
- +14+n	8-bit unsigned data samples for channel 1	
sample and (n-1) deltas (in order) for channel 1.
(optional, may not be present in single channel data)

- ...etc...	...etc...	Samples for additional channels may be present
