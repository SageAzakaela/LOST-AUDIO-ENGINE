#include "../src/PluginProcessor.h"

#include <iostream>

namespace
{
juce::TextButton* findButton(juce::Component& component, const juce::String& text)
{
    if (auto* button = dynamic_cast<juce::TextButton*>(&component); button != nullptr && button->getButtonText() == text)
        return button;
    for (auto* child : component.getChildren())
        if (auto* result = findButton(*child, text)) return result;
    return nullptr;
}

bool saveView(juce::AudioProcessorEditor& editor, const juce::File& output)
{
    const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    output.getParentDirectory().createDirectory();
    juce::FileOutputStream stream(output);
    juce::PNGImageFormat format;
    return stream.openedOk() && format.writeImageToStream(image, stream);
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) return 2;
    const juce::File outputDirectory(juce::String::fromUTF8(argv[1]));
    TransmissionEngineAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(980, 660);

    const struct { const char* button; const char* file; } views[] {
        { "SIMPLE", "transmission-simple-980x660.png" },
        { "ADVANCED", "transmission-advanced-980x660.png" },
        { "PERFORMER", "transmission-performer-980x660.png" }
    };
    for (const auto& view : views)
    {
        auto* button = findButton(*editor, view.button);
        if (button == nullptr || !button->onClick)
        {
            std::cerr << "Missing editor mode button: " << view.button << '\n';
            return 1;
        }
        button->onClick();
        editor->resized();
        if (!saveView(*editor, outputDirectory.getChildFile(view.file))) return 1;
    }
    std::cout << "Rendered Transmission Simple, Advanced, and Performer at 980x660\n";
}
