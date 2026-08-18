#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace
{
const juce::Colour kBackground { 0xffd9d5cc };
const juce::Colour kPanel { 0xffebe8e1 };
const juce::Colour kFloor { 0xffd1cec5 };
const juce::Colour kGraphite { 0xff2c2d2b };
const juce::Colour kMutedGraphite { 0xff676862 };
const juce::Colour kAccent { 0xff6c8275 };
const juce::Colour kWarmAccent { 0xffa66f45 };

constexpr float kFloorRangeMetres = 18.0f;
constexpr int kMatrixRowHeight = 30;
constexpr int kMatrixHeaderHeight = 30;
constexpr int kMatrixSourceWidth = 176;
constexpr int kMatrixSpeakerWidth = 88;

[[nodiscard]] juce::String speakerTypeText(clubcraft::SpeakerType type)
{
    switch (type)
    {
        case clubcraft::SpeakerType::sub: return "SUB";
        case clubcraft::SpeakerType::woofer: return "WOOFER";
        case clubcraft::SpeakerType::mid: return "MID";
        case clubcraft::SpeakerType::high: return "HIGH";
        case clubcraft::SpeakerType::fullRange: return "FULL RANGE";
    }
    return "FULL RANGE";
}

[[nodiscard]] clubcraft::SpeakerType speakerTypeFromCombo(int selectedId)
{
    switch (selectedId)
    {
        case 1: return clubcraft::SpeakerType::sub;
        case 2: return clubcraft::SpeakerType::woofer;
        case 4: return clubcraft::SpeakerType::mid;
        case 5: return clubcraft::SpeakerType::high;
        default: return clubcraft::SpeakerType::fullRange;
    }
}

[[nodiscard]] int comboIdForSpeakerType(clubcraft::SpeakerType type)
{
    switch (type)
    {
        case clubcraft::SpeakerType::sub: return 1;
        case clubcraft::SpeakerType::woofer: return 2;
        case clubcraft::SpeakerType::fullRange: return 3;
        case clubcraft::SpeakerType::mid: return 4;
        case clubcraft::SpeakerType::high: return 5;
    }
    return 3;
}

[[nodiscard]] bool matchesSearch(const juce::String& text, const juce::String& search)
{
    return search.isEmpty() || text.toLowerCase().contains(search.toLowerCase());
}
}

class ClubCraftPhase0AudioProcessorEditor::FloorView final : public juce::Component
{
public:
    using SpeakerSelected = std::function<void(const std::string&)>;

    FloorView(ClubCraftPhase0AudioProcessor& processorToUse, SpeakerSelected onSpeakerSelected)
        : pluginProcessor(processorToUse), onSelect(std::move(onSpeakerSelected))
    {
        setInterceptsMouseClicks(true, true);
    }

    void setSelectedSpeaker(std::string stableId)
    {
        selectedSpeakerId = std::move(stableId);
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
        graphics.setColour(kFloor);
        graphics.fillRoundedRectangle(bounds, 8.0f);
        graphics.setColour(kGraphite.withAlpha(0.16f));
        graphics.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        const auto centre = bounds.getCentre();
        graphics.setColour(kGraphite.withAlpha(0.10f));
        graphics.drawLine(bounds.getX(), centre.y, bounds.getRight(), centre.y, 1.0f);
        graphics.drawLine(centre.x, bounds.getY(), centre.x, bounds.getBottom(), 1.0f);
        graphics.setColour(kMutedGraphite.withAlpha(0.65f));
        graphics.setFont(11.0f);
        graphics.drawText("STAGE / SOURCE ORIGIN", juce::Rectangle<float>(centre.x - 82.0f, centre.y - 18.0f, 164.0f, 16.0f),
                          juce::Justification::centred);
        graphics.fillEllipse(centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);

        const auto scene = pluginProcessor.copyDynamicScene();
        for (const auto& speaker : scene.speakers)
        {
            const auto point = worldToScreen(speaker.position, bounds);
            const auto isSelected = speaker.stableId == selectedSpeakerId;
            graphics.setColour(isSelected ? kAccent : kGraphite);
            graphics.fillEllipse(point.x - 10.0f, point.y - 10.0f, 20.0f, 20.0f);
            graphics.setColour(kPanel);
            graphics.setFont(9.0f);
            graphics.drawText(speakerTypeText(speaker.type).substring(0, 1),
                              juce::Rectangle<float>(point.x - 8.0f, point.y - 7.0f, 16.0f, 14.0f),
                              juce::Justification::centred);
            graphics.setColour(kGraphite);
            graphics.setFont(10.0f);
            graphics.drawText(juce::String(speaker.name),
                              juce::Rectangle<float>(point.x - 45.0f, point.y + 12.0f, 90.0f, 15.0f),
                              juce::Justification::centred);
        }

        const auto listener = worldToScreen(scene.listener, bounds);
        graphics.setColour(kWarmAccent);
        juce::Path listenerPath;
        listenerPath.addTriangle(listener.x, listener.y - 11.0f, listener.x - 9.0f, listener.y + 8.0f,
                                 listener.x + 9.0f, listener.y + 8.0f);
        graphics.fillPath(listenerPath);
        graphics.setColour(kGraphite);
        graphics.setFont(10.0f);
        graphics.drawText("LISTENER", juce::Rectangle<float>(listener.x - 40.0f, listener.y + 12.0f, 80.0f, 14.0f),
                          juce::Justification::centred);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto scene = pluginProcessor.copyDynamicScene();
        const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
        const auto point = event.position;
        const auto listenerPoint = worldToScreen(scene.listener, bounds);
        if (point.getDistanceFrom(listenerPoint) < 18.0f)
        {
            dragKind = DragKind::listener;
            return;
        }

        for (auto iterator = scene.speakers.rbegin(); iterator != scene.speakers.rend(); ++iterator)
        {
            if (point.getDistanceFrom(worldToScreen(iterator->position, bounds)) < 18.0f)
            {
                dragKind = DragKind::speaker;
                draggedSpeakerId = iterator->stableId;
                onSelect(draggedSpeakerId);
                return;
            }
        }
        dragKind = DragKind::none;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        applyDrag(event.position, false);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        applyDrag(event.position, true);
        dragKind = DragKind::none;
        draggedSpeakerId.clear();
    }

private:
    enum class DragKind { none, speaker, listener };

    [[nodiscard]] static juce::Point<float> worldToScreen(clubcraft::PlanarPosition position,
                                                            const juce::Rectangle<float>& bounds)
    {
        const auto centre = bounds.getCentre();
        return {
            centre.x + position.x * bounds.getWidth() / (2.0f * kFloorRangeMetres),
            centre.y - position.y * bounds.getHeight() / (2.0f * kFloorRangeMetres),
        };
    }

    [[nodiscard]] static clubcraft::PlanarPosition screenToWorld(juce::Point<float> point,
                                                                   const juce::Rectangle<float>& bounds)
    {
        const auto centre = bounds.getCentre();
        return {
            juce::jlimit(-kFloorRangeMetres, kFloorRangeMetres,
                         (point.x - centre.x) * (2.0f * kFloorRangeMetres) / bounds.getWidth()),
            juce::jlimit(-kFloorRangeMetres, kFloorRangeMetres,
                         (centre.y - point.y) * (2.0f * kFloorRangeMetres) / bounds.getHeight()),
        };
    }

    void applyDrag(juce::Point<float> point, bool finalPosition)
    {
        if (dragKind == DragKind::none)
            return;
        const auto position = screenToWorld(point, getLocalBounds().toFloat().reduced(8.0f));
        if (dragKind == DragKind::listener)
            static_cast<void>(pluginProcessor.moveListener(position, finalPosition));
        else if (!draggedSpeakerId.empty())
            static_cast<void>(pluginProcessor.moveSpeaker(draggedSpeakerId, position, finalPosition));
        repaint();
    }

    ClubCraftPhase0AudioProcessor& pluginProcessor;
    SpeakerSelected onSelect;
    std::string selectedSpeakerId;
    std::string draggedSpeakerId;
    DragKind dragKind = DragKind::none;
};

class ClubCraftPhase0AudioProcessorEditor::RoutingMatrix final : public juce::Component
{
public:
    using Changed = std::function<void()>;
    using MaterialiseRequested = std::function<void()>;

    RoutingMatrix(ClubCraftPhase0AudioProcessor& processorToUse, Changed onChanged,
                  MaterialiseRequested onMaterialiseRequested)
        : pluginProcessor(processorToUse), onSceneChanged(std::move(onChanged)),
          onMaterialise(std::move(onMaterialiseRequested))
    {
    }

    void update(const clubcraft::DynamicScene& newScene,
                const std::vector<clubcraft::SourceRegistration>& sources,
                const juce::String& search)
    {
        scene = newScene;
        sourceRows.clear();
        std::unordered_set<std::string> knownIds;
        for (const auto& source : sources)
        {
            sourceRows.push_back({ source.sourceId, source.displayName, false });
            knownIds.insert(source.sourceId);
        }
        for (const auto& route : scene.routes)
        {
            if (knownIds.insert(route.sourceId).second)
                sourceRows.push_back({ route.sourceId, "OFFLINE / " + route.sourceId, true });
        }
        std::sort(sourceRows.begin(), sourceRows.end(), [](const SourceRow& lhs, const SourceRow& rhs)
        {
            if (lhs.name == rhs.name)
                return lhs.sourceId < rhs.sourceId;
            return lhs.name < rhs.name;
        });
        sourceRows.erase(std::remove_if(sourceRows.begin(), sourceRows.end(), [&search](const SourceRow& source)
        {
            return !matchesSearch(juce::String(source.name) + " " + source.sourceId, search);
        }), sourceRows.end());
        setSize(kMatrixSourceWidth + static_cast<int>(scene.speakers.size()) * kMatrixSpeakerWidth,
                kMatrixHeaderHeight + static_cast<int>(sourceRows.size()) * kMatrixRowHeight);
        repaint();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(kPanel);
        const auto bounds = getLocalBounds();
        graphics.setColour(kGraphite.withAlpha(0.07f));
        graphics.fillRect(0, 0, bounds.getWidth(), kMatrixHeaderHeight);
        graphics.setColour(kMutedGraphite);
        graphics.setFont(10.0f);
        graphics.drawText("SOURCE", 8, 0, kMatrixSourceWidth - 12, kMatrixHeaderHeight,
                          juce::Justification::centredLeft);
        for (std::size_t speakerIndex = 0; speakerIndex < scene.speakers.size(); ++speakerIndex)
        {
            const auto& speaker = scene.speakers[speakerIndex];
            const auto x = kMatrixSourceWidth + static_cast<int>(speakerIndex) * kMatrixSpeakerWidth;
            graphics.drawFittedText(juce::String(speaker.name), x + 4, 0, kMatrixSpeakerWidth - 8, kMatrixHeaderHeight,
                                    juce::Justification::centred, 1);
        }

        const auto clip = graphics.getClipBounds();
        const auto firstRow = std::max(0, (clip.getY() - kMatrixHeaderHeight) / kMatrixRowHeight);
        const auto lastRow = std::min(static_cast<int>(sourceRows.size()),
                                      (clip.getBottom() - kMatrixHeaderHeight + kMatrixRowHeight - 1) / kMatrixRowHeight);
        for (int row = firstRow; row < lastRow; ++row)
        {
            const auto y = kMatrixHeaderHeight + row * kMatrixRowHeight;
            const auto& source = sourceRows[static_cast<std::size_t>(row)];
            graphics.setColour(source.offline ? kMutedGraphite.withAlpha(0.15f) : kGraphite.withAlpha(0.04f));
            graphics.fillRect(0, y, bounds.getWidth(), kMatrixRowHeight - 1);
            graphics.setColour(source.offline ? kMutedGraphite : kGraphite);
            graphics.setFont(11.0f);
            graphics.drawFittedText(juce::String(source.name), 8, y, kMatrixSourceWidth - 12, kMatrixRowHeight,
                                    juce::Justification::centredLeft, 1);

            for (std::size_t speakerIndex = 0; speakerIndex < scene.speakers.size(); ++speakerIndex)
            {
                const auto x = kMatrixSourceWidth + static_cast<int>(speakerIndex) * kMatrixSpeakerWidth;
                const auto isRouted = hasFullRoute(source.sourceId, scene.speakers[speakerIndex].stableId);
                const auto cell = juce::Rectangle<float>(static_cast<float>(x + 8), static_cast<float>(y + 6),
                                                          static_cast<float>(kMatrixSpeakerWidth - 16), 18.0f);
                graphics.setColour(isRouted ? kAccent : kGraphite.withAlpha(0.12f));
                graphics.fillRoundedRectangle(cell, 9.0f);
                graphics.setColour(isRouted ? kPanel : kMutedGraphite);
                graphics.setFont(10.0f);
                graphics.drawText(isRouted ? "ON" : "—", cell, juce::Justification::centred);
            }
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        const auto point = event.getPosition();
        const auto row = (point.y - kMatrixHeaderHeight) / kMatrixRowHeight;
        const auto column = (point.x - kMatrixSourceWidth) / kMatrixSpeakerWidth;
        if (row < 0 || column < 0 || row >= static_cast<int>(sourceRows.size())
            || column >= static_cast<int>(scene.speakers.size()))
            return;
        if (scene.legacyDefaultRouting)
        {
            onMaterialise();
            return;
        }

        const auto& source = sourceRows[static_cast<std::size_t>(row)];
        const auto& speaker = scene.speakers[static_cast<std::size_t>(column)];
        const auto enabled = !hasFullRoute(source.sourceId, speaker.stableId);
        if (pluginProcessor.setFullRouteEnabled(source.sourceId, speaker.stableId, enabled))
            onSceneChanged();
    }

private:
    struct SourceRow
    {
        std::string sourceId;
        std::string name;
        bool offline = false;
    };

    [[nodiscard]] bool hasFullRoute(const std::string& sourceId, const std::string& speakerId) const
    {
        return std::any_of(scene.routes.begin(), scene.routes.end(), [&sourceId, &speakerId](const auto& route)
        {
            return route.sourceId == sourceId && route.speakerStableId == speakerId
                && route.mode == clubcraft::RouteMode::full && route.inputMode == clubcraft::InputChannelMode::sumMono;
        });
    }

    ClubCraftPhase0AudioProcessor& pluginProcessor;
    Changed onSceneChanged;
    MaterialiseRequested onMaterialise;
    clubcraft::DynamicScene scene;
    std::vector<SourceRow> sourceRows;
};

ClubCraftPhase0AudioProcessorEditor::ClubCraftPhase0AudioProcessorEditor(
    ClubCraftPhase0AudioProcessor& processorToUse)
    : AudioProcessorEditor(&processorToUse), pluginProcessor(processorToUse)
{
    setSize(1180, 820);
    setResizable(true, true);
    setResizeLimits(860, 620, 1600, 1200);

    titleLabel.setText("CLUB CRAFT", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(20.0f).withKerningFactor(0.18f));
    titleLabel.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(titleLabel);

    roleLabel.setText("ROLE", juce::dontSendNotification);
    roleLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.10f));
    roleLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    roleLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(roleLabel);

    roleSelector.addItem("SOURCE", 1);
    roleSelector.addItem("CLUB", 2);
    roleSelector.setJustificationType(juce::Justification::centred);
    roleSelector.setColour(juce::ComboBox::backgroundColourId, kPanel);
    roleSelector.setColour(juce::ComboBox::outlineColourId, kGraphite.withAlpha(0.18f));
    roleSelector.setColour(juce::ComboBox::textColourId, kGraphite);
    addAndMakeVisible(roleSelector);

    sessionLabel.setFont(juce::FontOptions(12.0f));
    sessionLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(sessionLabel);
    connectionLabel.setFont(juce::FontOptions(12.0f));
    connectionLabel.setColour(juce::Label::textColourId, kAccent);
    connectionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(connectionLabel);

    configureDbSlider(masterSlider, masterLabel, "MASTER");
    configureToneSlider();

    floorLabel.setText("FLOOR VIEW  /  FIXED STAGE ORIGIN", juce::dontSendNotification);
    floorLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    floorLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(floorLabel);
    floorView = std::make_unique<FloorView>(pluginProcessor, [this](const std::string& stableId) { selectSpeaker(stableId); });
    addAndMakeVisible(*floorView);

    for (auto* button : { &addSpeakerButton, &deleteSpeakerButton, &materialiseButton })
    {
        button->setColour(juce::TextButton::buttonColourId, kGraphite);
        button->setColour(juce::TextButton::textColourOffId, kPanel);
        addAndMakeVisible(*button);
    }
    addSpeakerButton.onClick = [this] { addSpeaker(); };
    deleteSpeakerButton.onClick = [this] { deleteSelectedSpeaker(); };
    materialiseButton.onClick = [this] { materialiseLegacyRouting(); };

    selectedSpeakerLabel.setFont(juce::FontOptions(12.0f).withKerningFactor(0.06f));
    selectedSpeakerLabel.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(selectedSpeakerLabel);
    selectedSpeakerTypeLabel.setText("TYPE", juce::dontSendNotification);
    selectedSpeakerTypeLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    selectedSpeakerTypeLabel.setFont(juce::FontOptions(10.0f));
    addAndMakeVisible(selectedSpeakerTypeLabel);
    selectedSpeakerType.addItem("SUB", 1);
    selectedSpeakerType.addItem("WOOFER", 2);
    selectedSpeakerType.addItem("FULL RANGE", 3);
    selectedSpeakerType.addItem("MID", 4);
    selectedSpeakerType.addItem("HIGH", 5);
    selectedSpeakerType.setColour(juce::ComboBox::backgroundColourId, kPanel);
    selectedSpeakerType.setColour(juce::ComboBox::outlineColourId, kGraphite.withAlpha(0.18f));
    addAndMakeVisible(selectedSpeakerType);
    selectedSpeakerType.onChange = [this]
    {
        if (updatingInspector || selectedSpeakerId.empty())
            return;
        const auto type = speakerTypeFromCombo(selectedSpeakerType.getSelectedId());
        static_cast<void>(pluginProcessor.editDynamicScene([this, type](clubcraft::DynamicScene& scene)
        {
            const auto speaker = std::find_if(scene.speakers.begin(), scene.speakers.end(), [this](const auto& item)
            { return item.stableId == selectedSpeakerId; });
            if (speaker == scene.speakers.end()) return false;
            speaker->type = type;
            return true;
        }, true));
    };

    configureDbSlider(selectedSpeakerLevel, selectedSpeakerLevelLabel, "LEVEL");
    selectedSpeakerLevel.onValueChange = [this]
    {
        if (updatingInspector || selectedSpeakerId.empty())
            return;
        const auto level = static_cast<float>(selectedSpeakerLevel.getValue());
        static_cast<void>(pluginProcessor.editDynamicScene([this, level](clubcraft::DynamicScene& scene)
        {
            const auto speaker = std::find_if(scene.speakers.begin(), scene.speakers.end(), [this](const auto& item)
            { return item.stableId == selectedSpeakerId; });
            if (speaker == scene.speakers.end()) return false;
            speaker->levelDb = level;
            return true;
        }, false));
    };
    selectedSpeakerLevel.onDragEnd = [this]
    {
        if (selectedSpeakerId.empty()) return;
        const auto level = static_cast<float>(selectedSpeakerLevel.getValue());
        static_cast<void>(pluginProcessor.editDynamicScene([this, level](clubcraft::DynamicScene& scene)
        {
            const auto speaker = std::find_if(scene.speakers.begin(), scene.speakers.end(), [this](const auto& item)
            { return item.stableId == selectedSpeakerId; });
            if (speaker == scene.speakers.end()) return false;
            speaker->levelDb = level;
            return true;
        }, true));
    };

    routingLabel.setText("FULL ROUTING MATRIX  /  SOURCE → SPEAKER", juce::dontSendNotification);
    routingLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.12f));
    routingLabel.setColour(juce::Label::textColourId, kMutedGraphite);
    addAndMakeVisible(routingLabel);
    matrixSearch.setTextToShowWhenEmpty("Search SOURCE", kMutedGraphite.withAlpha(0.75f));
    matrixSearch.setColour(juce::TextEditor::backgroundColourId, kPanel);
    matrixSearch.setColour(juce::TextEditor::outlineColourId, kGraphite.withAlpha(0.18f));
    matrixSearch.onTextChange = [this] { refreshDynamicUi(); };
    addAndMakeVisible(matrixSearch);

    routingMatrix = std::make_unique<RoutingMatrix>(pluginProcessor, [this] { refreshDynamicUi(); },
                                                     [this] { materialiseLegacyRouting(); });
    matrixViewport.setViewedComponent(routingMatrix.get(), false);
    matrixViewport.setScrollBarsShown(true, true);
    addAndMakeVisible(matrixViewport);

    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        pluginProcessor.getParameters(), "role", roleSelector);
    masterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "masterLevel", masterSlider);
    responseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        pluginProcessor.getParameters(), "genericResponseTone", responseSlider);

    refreshStatus();
    refreshDynamicUi();
    startTimerHz(15);
}

ClubCraftPhase0AudioProcessorEditor::~ClubCraftPhase0AudioProcessorEditor()
{
    stopTimer();
    matrixViewport.setViewedComponent(nullptr, false);
}

void ClubCraftPhase0AudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(kBackground);
    const auto panel = getLocalBounds().reduced(16).withTrimmedTop(64);
    graphics.setColour(kPanel);
    graphics.fillRoundedRectangle(panel.toFloat(), 9.0f);
    graphics.setColour(kGraphite.withAlpha(0.12f));
    graphics.drawRoundedRectangle(panel.toFloat(), 9.0f, 1.0f);
}

void ClubCraftPhase0AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    auto header = bounds.removeFromTop(34);
    titleLabel.setBounds(header.removeFromLeft(header.getWidth() / 2));
    roleSelector.setBounds(header.removeFromRight(120));
    roleLabel.setBounds(header);

    auto details = bounds.removeFromTop(30);
    sessionLabel.setBounds(details.removeFromLeft(details.getWidth() / 2));
    connectionLabel.setBounds(details);

    auto panel = bounds.withTrimmedTop(8).reduced(18);
    auto masterRow = panel.removeFromTop(34);
    masterLabel.setBounds(masterRow.removeFromLeft(150));
    masterSlider.setBounds(masterRow.reduced(3, 3));
    auto responseRow = panel.removeFromTop(34);
    responseLabel.setBounds(responseRow.removeFromLeft(150));
    responseSlider.setBounds(responseRow.reduced(3, 3));

    auto main = panel.withTrimmedTop(8);
    const auto leftWidth = static_cast<int>(main.getWidth() * 0.46f);
    auto left = main.removeFromLeft(leftWidth).withTrimmedRight(10);
    auto right = main.withTrimmedLeft(8);

    floorLabel.setBounds(left.removeFromTop(22));
    auto floorBounds = left.removeFromTop(std::max(230, left.getHeight() / 2));
    floorView->setBounds(floorBounds);
    auto buttons = left.removeFromTop(34);
    addSpeakerButton.setBounds(buttons.removeFromLeft(118).reduced(2));
    deleteSpeakerButton.setBounds(buttons.removeFromLeft(146).reduced(2));
    materialiseButton.setBounds(buttons.reduced(2));

    selectedSpeakerLabel.setBounds(left.removeFromTop(24));
    auto inspector = left.removeFromTop(34);
    selectedSpeakerTypeLabel.setBounds(inspector.removeFromLeft(34));
    selectedSpeakerType.setBounds(inspector.removeFromLeft(116).reduced(2));
    selectedSpeakerLevelLabel.setBounds(inspector.removeFromLeft(44));
    selectedSpeakerLevel.setBounds(inspector.reduced(2));

    routingLabel.setBounds(right.removeFromTop(22));
    matrixSearch.setBounds(right.removeFromTop(30).removeFromRight(180).reduced(2));
    matrixViewport.setBounds(right.withTrimmedTop(4));
}

void ClubCraftPhase0AudioProcessorEditor::timerCallback()
{
    refreshStatus();
    refreshDynamicUi();
}

void ClubCraftPhase0AudioProcessorEditor::configureDbSlider(juce::Slider& slider,
                                                             juce::Label& label,
                                                             const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::FontOptions(11.0f).withKerningFactor(0.08f));
    label.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 22);
    slider.setTextValueSuffix(" dB");
    slider.setColour(juce::Slider::trackColourId, kGraphite.withAlpha(0.20f));
    slider.setColour(juce::Slider::thumbColourId, kGraphite);
    slider.setColour(juce::Slider::backgroundColourId, kGraphite.withAlpha(0.10f));
    slider.setColour(juce::Slider::textBoxTextColourId, kGraphite);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);
}

void ClubCraftPhase0AudioProcessorEditor::configureToneSlider()
{
    responseLabel.setText("GENERIC RESPONSE", juce::dontSendNotification);
    responseLabel.setFont(juce::FontOptions(11.0f).withKerningFactor(0.08f));
    responseLabel.setColour(juce::Label::textColourId, kGraphite);
    addAndMakeVisible(responseLabel);
    responseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    responseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 22);
    responseSlider.textFromValueFunction = [] (double value)
    {
        return juce::String(static_cast<int>(std::round(value * 100.0))) + " %";
    };
    responseSlider.setColour(juce::Slider::trackColourId, kAccent.withAlpha(0.60f));
    responseSlider.setColour(juce::Slider::thumbColourId, kAccent);
    responseSlider.setColour(juce::Slider::backgroundColourId, kGraphite.withAlpha(0.10f));
    responseSlider.setColour(juce::Slider::textBoxTextColourId, kGraphite);
    responseSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    responseSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(responseSlider);
}

void ClubCraftPhase0AudioProcessorEditor::refreshStatus()
{
    const auto isClub = pluginProcessor.isClubRole();
    const auto editable = isClub && !pluginProcessor.hasClubConflict();
    sessionLabel.setText("SESSION  " + pluginProcessor.getSessionId(), juce::dontSendNotification);
    masterSlider.setEnabled(editable);
    responseSlider.setEnabled(editable);
    floorView->setEnabled(editable);
    addSpeakerButton.setEnabled(editable);
    deleteSpeakerButton.setEnabled(editable && !selectedSpeakerId.empty());
    selectedSpeakerType.setEnabled(editable && !selectedSpeakerId.empty());
    selectedSpeakerLevel.setEnabled(editable && !selectedSpeakerId.empty());
    matrixViewport.setEnabled(editable);
    materialiseButton.setVisible(editable && pluginProcessor.copyDynamicScene().legacyDefaultRouting);

    if (isClub)
    {
        connectionLabel.setText(pluginProcessor.hasClubConflict()
                ? "CLUB CONFLICT / another CLUB owns this Session"
                : "CLUB ACTIVE / Dynamic Scene Control Plane",
            juce::dontSendNotification);
    }
    else if (pluginProcessor.wasSourceRekeyed())
    {
        connectionLabel.setText("SOURCE ID REKEYED / waiting for explicit routes", juce::dontSendNotification);
    }
    else
    {
        connectionLabel.setText(pluginProcessor.isConnectedToClub()
                ? "SOURCE CONNECTED / follows CLUB route plan"
                : "WAITING FOR CLUB",
            juce::dontSendNotification);
    }
}

void ClubCraftPhase0AudioProcessorEditor::refreshDynamicUi()
{
    const auto scene = pluginProcessor.copyDynamicScene();
    if (selectedSpeakerId.empty() && !scene.speakers.empty())
        selectedSpeakerId = scene.speakers.front().stableId;
    if (!selectedSpeakerId.empty()
        && std::none_of(scene.speakers.begin(), scene.speakers.end(), [this](const auto& speaker)
        { return speaker.stableId == selectedSpeakerId; }))
        selectedSpeakerId.clear();

    floorView->setSelectedSpeaker(selectedSpeakerId);
    routingMatrix->update(scene, pluginProcessor.getKnownSources(), matrixSearch.getText());
    displayedSceneRevision = pluginProcessor.getControlSceneRevision();

    updatingInspector = true;
    const auto selected = std::find_if(scene.speakers.begin(), scene.speakers.end(), [this](const auto& speaker)
    { return speaker.stableId == selectedSpeakerId; });
    if (selected == scene.speakers.end())
    {
        selectedSpeakerLabel.setText("SELECT A SPEAKER", juce::dontSendNotification);
        selectedSpeakerType.setSelectedId(0, juce::dontSendNotification);
        selectedSpeakerLevel.setValue(0.0, juce::dontSendNotification);
    }
    else
    {
        selectedSpeakerLabel.setText("SELECTED  " + juce::String(selected->name), juce::dontSendNotification);
        selectedSpeakerType.setSelectedId(comboIdForSpeakerType(selected->type), juce::dontSendNotification);
        selectedSpeakerLevel.setValue(selected->levelDb, juce::dontSendNotification);
    }
    updatingInspector = false;
}

void ClubCraftPhase0AudioProcessorEditor::selectSpeaker(const std::string& stableId)
{
    selectedSpeakerId = stableId;
    refreshDynamicUi();
}

void ClubCraftPhase0AudioProcessorEditor::addSpeaker()
{
    const auto scene = pluginProcessor.copyDynamicScene();
    if (scene.legacyDefaultRouting)
    {
        materialiseLegacyRouting();
        return;
    }
    const auto offset = static_cast<float>(scene.speakers.size() % 4) * 2.0f;
    if (pluginProcessor.addSpeaker(clubcraft::SpeakerType::fullRange, { -3.0f + offset, -3.0f + offset }))
        refreshDynamicUi();
}

void ClubCraftPhase0AudioProcessorEditor::deleteSelectedSpeaker()
{
    if (selectedSpeakerId.empty())
        return;
    const auto scene = pluginProcessor.copyDynamicScene();
    if (scene.legacyDefaultRouting)
    {
        materialiseLegacyRouting();
        return;
    }
    if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::WarningIcon, "Delete speaker?",
                                            "This also removes all Routes connected to the selected Speaker.",
                                            "DELETE", "CANCEL", this, nullptr))
    {
        static_cast<void>(pluginProcessor.removeSpeaker(selectedSpeakerId));
        selectedSpeakerId.clear();
        refreshDynamicUi();
    }
}

void ClubCraftPhase0AudioProcessorEditor::materialiseLegacyRouting()
{
    const auto scene = pluginProcessor.copyDynamicScene();
    if (!scene.legacyDefaultRouting)
        return;
    const auto sourceCount = pluginProcessor.getKnownSources().size();
    const auto routeCount = sourceCount * std::min<std::size_t>(clubcraft::kSpeakerCount, scene.speakers.size());
    const auto message = "Convert legacy implicit 0.25 gain Routes into explicit Full Routes?\n\n"
        + juce::String(static_cast<int>(sourceCount)) + " Sources → " + juce::String(static_cast<int>(routeCount))
        + " Routes. This can be edited in the Routing Matrix afterwards.";
    if (juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon, "Materialise legacy routing",
                                            message, "MATERIALISE", "CANCEL", this, nullptr))
    {
        static_cast<void>(pluginProcessor.materialiseLegacyRouting());
        refreshDynamicUi();
    }
}
