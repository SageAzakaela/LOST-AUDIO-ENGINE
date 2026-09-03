#include "../src/PluginProcessor.h"
#include <iostream>

namespace
{
juce::TextButton* findButton(juce::Component& c,const juce::String& text){if(auto*b=dynamic_cast<juce::TextButton*>(&c);b&&b->getButtonText()==text)return b;for(auto*child:c.getChildren())if(auto*r=findButton(*child,text))return r;return nullptr;}
bool save(juce::AudioProcessorEditor&e,const juce::File&f){const auto image=e.createComponentSnapshot(e.getLocalBounds(),true,1);f.getParentDirectory().createDirectory();juce::FileOutputStream stream(f);juce::PNGImageFormat format;return stream.openedOk()&&format.writeImageToStream(image,stream);}
}

int main(int argc,char**argv)
{
    juce::ScopedJuceInitialiser_GUI gui;if(argc<2)return 2;const juce::File directory(juce::String::fromUTF8(argv[1]));OcclusionEngineAudioProcessor p;std::unique_ptr<juce::AudioProcessorEditor>e(p.createEditor());e->setSize(980,660);
    const struct{const char*button;const char*file;}views[]{{"SIMPLE","occlusion-simple-980x660.png"},{"ADVANCED","occlusion-advanced-980x660.png"},{"PERFORMER","occlusion-performer-980x660.png"}};
    for(const auto&v:views){auto*b=findButton(*e,v.button);if(!b||!b->onClick)return 1;b->onClick();e->resized();if(!save(*e,directory.getChildFile(v.file)))return 1;}
    std::cout<<"Rendered Occlusion Engine Simple, Advanced, and Performer at 980x660\n";
}
