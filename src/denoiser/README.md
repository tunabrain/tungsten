# NFOR Denoiser #

This is a public re-implementation of the NFOR denoiser described in the paper
[Nonlinearly Weighted First-order Regression for Denoising Monte Carlo Renderings](https://benedikt-bitterli.me/nfor/).

## Running the denoiser ##

In order to run the denoiser, you first need a Tungsten scene file that produces the appropriate output data. In the `rendering` block of a Tungsten scene file, make sure to specify auxiliary output buffers like this:

    "output_buffers": [
        {"type": "color",  "hdr_output_file": "color.exr",  "sample_variance": true, "two_buffer_variance": true},
        {"type": "depth",  "hdr_output_file": "depth.exr",  "sample_variance": true, "two_buffer_variance": true},
        {"type": "albedo", "hdr_output_file": "albedo.exr", "sample_variance": true, "two_buffer_variance": true},
        {"type": "normal", "hdr_output_file": "normal.exr", "sample_variance": true, "two_buffer_variance": true},
    ]
    
This will render the auxiliary feature buffers, including variance and half buffers, as the renderer runs. Once the render completes, you can produce the denoised result with

    denoiser scene.json output.exr

## Disclaimer ##

This is a re-implementation of the original source code used in the paper, and is not as polished as the original code. This version has some issues with residual noise and banding from the depth feature. These are bugs and not issues of the algorithm, but I do not currently have time to track down and fix these. Please keep this in mind if you intend to compare to results produced with this denoiser.

## Example Data ##

I've released 630 MB of renderings and feature buffers on 17 scenes, as well as results produced by this denoiser on that exact data. You may use this data in your research. An archive of this data is available here:
https://benedikt-bitterli.me/nfor/denoising-data.zip
