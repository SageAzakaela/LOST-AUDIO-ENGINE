#include "../src/PluginProcessor.h"
#include <iostream>
namespace
{
juce::TextButton* findButton(juce::Component& component, const juce::String& text) { if (auto* b = dynamic_cast<juce::TextButton*>(&component); b && b->getButtonText() == text) return b; for (auto* child : component.getChildren()) if (auto* result = findButton(*child, text)) return result; return nullptr; }
bool save(juce::AudioProcessorEditor& editor, const juce::File& output) { const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f); output.getParentDirectory().createDirectory(); juce::FileOutputStream stream(output); juce::PNGImageFormat format; return stream.openedOk() && format.writeImageToStream(image, stream); }
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui; if (argc < 2) return 2; const juce::File directory(juce::String::fromUTF8(argv[1])); CamcorderEngineAudioProcessor processor; std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor()); editor->setSize(980, 660);
    const struct { const char* button; const char* file; } views[] { { "SIMPLE", "camcorder-simple-980x660.png" }, { "ADVANCED", "camcorder-advanced-980x660.png" }, { "PERFORMER", "camcorder-performer-980x660.png" } };
    for (const auto& view : views) { auto* button = findButton(*editor, view.button); if (!button || !button->onClick) return 1; button->onClick(); editor->resized(); if (!save(*editor, directory.getChildFile(view.file))) return 1; }
    std::cout << "Rendered Camcorder Simple, Advanced, and Performer at 980x660\n";
}
