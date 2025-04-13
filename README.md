# mlwl-image-occlusion
SDL2 program to generate image occluded review items for [My Little Word Land](https://mylittlewordland.com/)

## Instructions
Dependencies: `libsdl2-dev libsdl2-image-dev libcurl4-openssl-dev tesseract jq`

Build with `make`

Install per-user with `make install`

Usage: `./mlwl-image-occlusion <image_path>`

For this to work, your MLWL course needs to be set up with Picture as the first column and Word as the second column.

## Future
- [ ] Cache credentials between runs
