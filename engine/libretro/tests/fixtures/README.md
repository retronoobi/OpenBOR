# WebM test fixtures

Synthetic color fields and sine waves, generated for this project; no game assets.
They are distributed under the same license as the core. FFmpeg is only needed
to regenerate these files, not to build or run the tests.

```sh
ffmpeg -f lavfi -i color=c=red:s=16x16:r=15:d=1 -f lavfi -i 'aevalsrc=0.25*sin(2*PI*440*t)|-0.25*sin(2*PI*440*t):s=48000:d=1' -c:v libvpx -b:v 40k -c:a libvorbis -shortest stereo.webm
ffmpeg -f lavfi -i color=c=blue:s=16x16:r=24:d=0.5 -an -c:v libvpx silent.webm
ffmpeg -f lavfi -i color=c=green:s=16x16:r=15:d=0.5 -f lavfi -i sine=frequency=660:sample_rate=32000:duration=0.5 -c:v libvpx -c:a libvorbis -shortest mono.webm
```
