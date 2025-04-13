# mlwl-image-occlusion
SDL2 program to generate image occluded review items for [My Little Word Land](https://mylittlewordland.com/)

https://github.com/user-attachments/assets/1f64ca5b-c261-48dc-b06b-74b433dfcb8e

becomes...

![screenshot](https://github.com/user-attachments/assets/ba604283-ab32-45d4-a6aa-8c6ce66402a0)

## Instructions
Dependencies: `libsdl2-dev libsdl2-image-dev libcurl4-openssl-dev curl tesseract jq`

Build with `make`

Install per-user with `make install`

Usage: `mlwl-image-occlusion <image_path>`

Your MLWL course needs to be set up with Picture as the first column and Word as the second column. Please also note that after running, it can take a minute to update everything on MLWL (which is why the script runs in the background until it's complete).

## Future
- [ ] Cache credentials between runs
