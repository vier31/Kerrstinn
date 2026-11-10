// Renders the editor with a loop loaded to <out>_1x.png and <out>_2x.png.
// usage: gui_snapshot loop.wav out
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    if (argc < 3)
        return 1;

    KerrStinnProcessor processor;
    processor.apvts.getParameter ("generator")->setValueNotifyingHost (4.0f / 25.0f);
    processor.apvts.getParameter ("slices")->setValueNotifyingHost (0.25f);
    processor.loadLoop (juce::File (argv[1]));
    processor.waitUntilIdle();

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    for (int scale : { 1, 2 })
    {
        editor->setSize (482 * scale, 254 * scale);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::File out (juce::String (argv[2]) + "_" + juce::String (scale) + "x.png");
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }
    editor = nullptr;
    return 0;
}
