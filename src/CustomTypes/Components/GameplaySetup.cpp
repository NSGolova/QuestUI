#include "CustomTypes/Components/GameplaySetup.hpp"

#include "BeatSaberUI.hpp"
#include "GlobalNamespace/CampaignFlowCoordinator.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/GameServerLobbyFlowCoordinator.hpp"
#include "GlobalNamespace/GameplaySetupViewController.hpp"

#include "GlobalNamespace/PlayerSettingsPanelController.hpp"
#include "GlobalNamespace/GameplayModifiersPanelController.hpp"
#include "GlobalNamespace/MultiplayerSettingsPanelController.hpp"
#include "GlobalNamespace/ColorsOverrideSettingsPanelController.hpp"
#include "GlobalNamespace/EnvironmentOverrideSettingsPanelController.hpp"

#include "HMUI/TextSegmentedControl.hpp"
#include "Sprites/carats.hpp"

DEFINE_TYPE(QuestUI, GameplaySetup);
DEFINE_TYPE(QuestUI, GameplaySetupTabMB);

static ConstString BaseGameplaySetupWrapper("BaseGameplaySetupWrapper");
static ConstString QuestuiGameplaySetupWrapper("QuestuiGameplaySetupWrapper");

extern Logger& getLogger();

namespace QuestUI
{
    void GameplaySetup::ctor()
    {
        INVOKE_CTOR();
    }
    
    void GameplaySetup::Setup()
    {
        for(auto& info : GameplaySetupMenuTabs::get()) {
            info->gameObject = nullptr;
            info->activatedBefore = false;
        }

        auto canvas = BeatSaberUI::CreateCanvas();
        canvas->get_transform()->SetParent(get_transform(), false);
        auto controlRect = reinterpret_cast<UnityEngine::RectTransform*>(canvas->get_transform());
        ArrayW<StringW> options(2);
        options[0] = "vanilla"; options[1] = "mods";
        moddedController = BeatSaberUI::CreateTextSegmentedControl(controlRect, {0, 0}, {80, 5.5}, options, std::bind(&GameplaySetup::SwitchGameplayTab, this, std::placeholders::_1));
        auto layout = moddedController->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>();
        layout->set_preferredWidth(100);

        controlRect->set_anchoredPosition({0, 45});
        controlRect->set_sizeDelta({80, 8.5});
        controlRect->set_localScale({1, 1, 1});
        
        // all the base game stuff will be on this
        auto baseGameRect = UnityEngine::GameObject::New_ctor()->AddComponent<UnityEngine::RectTransform*>();
        baseGameRect->get_gameObject()->set_name(BaseGameplaySetupWrapper);
        baseGameRect->SetParent(get_transform(), false);
        get_transform()->GetChild(0)->SetParent(baseGameRect, true);
        auto self = GetComponent<GlobalNamespace::GameplaySetupViewController*>();
        self->selectionSegmentedControl->get_transform()->SetParent(baseGameRect, true);            
        self->playerSettingsPanelController->get_transform()->SetParent(baseGameRect, true);            
        self->gameplayModifiersPanelController->get_transform()->SetParent(baseGameRect, true);            
        self->multiplayerSettingsPanelController->get_transform()->SetParent(baseGameRect, true);            
        self->environmentOverrideSettingsPanelController->get_transform()->SetParent(baseGameRect, true);            
        self->colorsOverrideSettingsPanelController->get_transform()->SetParent(baseGameRect, true);        

        // all the custom stuff will live hereg
        auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(get_transform());
        vertical->set_childAlignment(UnityEngine::TextAnchor::UpperLeft);
        vertical->set_childForceExpandHeight(false);
        vertical->set_childForceExpandWidth(false);
        vertical->set_childControlHeight(false);
        
        auto customRect = vertical->get_rectTransform();
        
        customRect->get_gameObject()->set_name(QuestuiGameplaySetupWrapper);
        customRect->get_transform()->SetParent(get_transform(), false);
        controlRect->SetAsFirstSibling();
        vertical->get_gameObject()->SetActive(false);
        auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(vertical->get_transform());
        horizontal->set_childForceExpandWidth(true);
        segmentedController = BeatSaberUI::CreateTextSegmentedControl(horizontal->get_transform(), {0, 0}, {80, 5.5}, ArrayW<StringW>(static_cast<il2cpp_array_size_t>(0)), std::bind(&GameplaySetup::ChooseModSegment, this, std::placeholders::_1));
        segmentedController->get_gameObject()->GetComponent<UnityEngine::UI::HorizontalLayoutGroup*>()->set_childForceExpandWidth(true);
        float buttonSize = 8.0f;
        auto left = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "", "SettingsButton", UnityEngine::Vector2(0, 0), UnityEngine::Vector2(buttonSize, buttonSize), [this] { MoveModMenus(-2); });
        left->get_transform()->SetAsFirstSibling();
        auto right = BeatSaberUI::CreateUIButton(horizontal->get_transform(), "", "SettingsButton", UnityEngine::Vector2(0, 0), UnityEngine::Vector2(buttonSize, buttonSize), [this] { MoveModMenus(2); });
        right->get_transform()->SetAsLastSibling();

        reinterpret_cast<UnityEngine::RectTransform*>(left->get_transform()->GetChild(0))->set_sizeDelta({buttonSize, buttonSize});
        reinterpret_cast<UnityEngine::RectTransform*>(right->get_transform()->GetChild(0))->set_sizeDelta({buttonSize, buttonSize});
        auto carat_left_sprite = BeatSaberUI::Base64ToSprite(carat_left);
        auto carat_right_sprite = BeatSaberUI::Base64ToSprite(carat_right);
        auto carat_left_inactive_sprite = BeatSaberUI::Base64ToSprite(carat_left_inactive);
        auto carat_right_inactive_sprite = BeatSaberUI::Base64ToSprite(carat_right_inactive);
        BeatSaberUI::SetButtonSprites(left, carat_left_inactive_sprite, carat_left_sprite);
        BeatSaberUI::SetButtonSprites(right, carat_right_inactive_sprite, carat_right_sprite);

        leftButton = left;
        rightButton = right;
    }

    void GameplaySetup::MoveModMenus(int offset)
    {
        int size = currentTabs.size();
        if (size == 0) return;
        // only clamp if size larger
        if (size > tabCount)
        {
            currentFirst += offset;
            currentFirst = std::clamp<int>(currentFirst, 0, size - tabCount);
        }

        SetModTexts();
        ChooseModSegment(0);
    }

    void GameplaySetup::Activate(bool firstActivation)
    {
        getLogger().debug("GameplaySetup::Activate");
        if (firstActivation) Setup();

        currentTabs = GameplaySetupMenuTabs::get(GetMenuType());

        SetModTexts();

        if (!currentTabs.empty()) {
            moddedController->get_gameObject()->SetActive(true);

            // this is a bad fix
            int selectedIndex = std::clamp<int>(segmentedController->segmentedControl->get_selectedCellNumber(), 0, currentTabs.size() - 1);

            currentMenu = currentTabs[currentFirst + selectedIndex];
            getLogger().debug("Current first %i segment %i", currentFirst, segmentedController->segmentedControl->get_selectedCellNumber());
            CRASH_UNLESS(currentMenu);
            // NULL HERE?
            for (auto& tab : currentTabs)
            {
                if (tab != currentMenu && tab->gameObject && tab->gameObject->get_active())
                {
                    tab->Deactivate();
                }
            }

            if (!currentMenu->gameObject) currentMenu->CreateObject(get_transform()->Find(QuestuiGameplaySetupWrapper));
            currentMenu->Activate();
        } else {
            moddedController->get_gameObject()->SetActive(false);
        }
    }

    void GameplaySetup::OnDisable()
    {
        moddedController->segmentedControl->SelectCellWithNumber(0);
        SwitchGameplayTab(0);
        moddedController->get_gameObject()->SetActive(false);
        if (currentMenu && currentMenu->gameObject) {
            currentMenu->gameObject->SetActive(false);
        }
    }

    Register::MenuType GameplaySetup::GetMenuType()
    {
        auto flowCoordinator = BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        auto klass = il2cpp_functions::object_get_class(reinterpret_cast<Il2CppObject*>(flowCoordinator));

        if (il2cpp_functions::class_is_assignable_from(classof(GlobalNamespace::CampaignFlowCoordinator*), klass))
            return Register::MenuType::Campaign;
        else if (il2cpp_functions::class_is_assignable_from(classof(GlobalNamespace::SinglePlayerLevelSelectionFlowCoordinator*), klass))
            return Register::MenuType::Solo;
        else if (il2cpp_functions::class_is_assignable_from(classof(GlobalNamespace::GameServerLobbyFlowCoordinator*), klass))
            return Register::MenuType::Online;
        else return Register::MenuType::Custom;
    }

    void GameplaySetup::SwitchGameplayTab(int idx)
    {
        get_transform()->Find(BaseGameplaySetupWrapper)->get_gameObject()->SetActive(idx == 0);
        get_transform()->Find(QuestuiGameplaySetupWrapper)->get_gameObject()->SetActive(idx == 1);
    }

    std::vector<int> GameplaySetup::get_slice()
    {
        std::vector<int> slice = {};
        int size = currentTabs.size();
        if (size > tabCount)
        {
            // more tabs than fit
            for (int i = 0; i < tabCount; i++) {
                auto current = currentFirst + i;
                // only allow elements that are in bounds
                if (current < size)
                    slice.push_back(currentFirst + i);
            }
        } 
        else
        {
            // equal or less tabs than fit
            for (int i = 0; i < size; i++) {
                slice.push_back(i);
            }
        }

        return slice;
    }

    void GameplaySetup::SetModTexts()
    {
        auto slice = get_slice();

        ArrayW<StringW> texts(slice.size());

        for (int i = 0; i < slice.size(); i++)
        {
            texts[i] = currentTabs[slice[i]]->title;
        }

        segmentedController->set_texts(texts);
    }

    void GameplaySetup::ChooseModSegment(int idx)
    {
        int index = currentFirst + idx;
        if (index >= currentTabs.size()) index = 0;
        auto nextMenu = currentTabs[index];
        if (currentMenu == nextMenu) return;
        
        for (auto& tab : currentTabs)
        {
            if (tab->gameObject && tab->gameObject->get_active())
            {
                tab->Deactivate();
            }
        }

        if (!nextMenu->gameObject) nextMenu->CreateObject(get_transform()->Find(QuestuiGameplaySetupWrapper));
        nextMenu->Activate();
        currentMenu = nextMenu;
    }

    void GameplaySetupTabMB::Init(GameplaySetupMenuTabs::GameplaySetupMenu *assignedMenu) {
        this->assignedMenu = assignedMenu;
    }

    void GameplaySetupTabMB::OnDestroy() {
        assignedMenu->gameObject = nullptr;
    }
}