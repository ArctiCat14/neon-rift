/*
    Editor smoke test: builds the real editor, snapshots it to PNG and tears it
    down. Used to reproduce the pluginval editor crash and to generate the
    README screenshot.
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstdio>

int main (int, char**)
{
    std::printf ("step 1: init GUI\n");
    std::fflush (stdout);
    juce::ScopedJuceInitialiser_GUI init;

    auto processor = std::make_unique<NeonRiftAudioProcessor>();
    std::printf ("step 2: processor created\n");
    std::fflush (stdout);

    processor->prepareToPlay (48000.0, 512);
    std::printf ("step 3: prepared\n");
    std::fflush (stdout);

    auto* editor = processor->createEditor();
    std::printf ("step 4: editor = %p\n", (void*) editor);
    std::fflush (stdout);

    editor->setSize (1000, 700);
    std::printf ("step 5: sized %d x %d\n", editor->getWidth(), editor->getHeight());
    std::fflush (stdout);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), false);
    std::printf ("step 6: snapshot %d x %d\n", image.getWidth(), image.getHeight());
    std::fflush (stdout);

    auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("NeonRift-editor.png");
    file.deleteFile();

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        juce::PNGImageFormat png;
        const bool ok = png.writeImageToStream (image, *stream);
        std::printf ("step 7: png written = %d -> %s\n", (int) ok, file.getFullPathName().toRawUTF8());
        std::fflush (stdout);
    }

    delete editor;
    std::printf ("step 8: editor deleted\n");
    std::fflush (stdout);

    processor.reset();
    std::printf ("step 9: processor deleted\n");
    std::fflush (stdout);

    return 0;
}
