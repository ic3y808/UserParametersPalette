#include <Core/CoreAll.h>
#include <Fusion/FusionAll.h>
#include <thread>
#include "nlohmann/json.hpp"
#include "UserParams.h"

using namespace adsk::core;
using namespace adsk::fusion;
using json = nlohmann::json;

Ptr<Application> _app;
Ptr<Product> _product;
Ptr<Design> _design;
Ptr<UserParameters> _parameters;
Ptr<UnitsManager> _units_manager;
Ptr<UserInterface> _ui;
size_t num;
Ptr<Palette> _palette;
Ptr<CustomEvent> customEvent;

json getParameterJSON()
{
	_app = Application::get();
	if (!_app)
		return "";

	_product = _app->activeProduct();
	if (!_product)
		return "";

	_design = _app->activeProduct();
	if (!_product)
		return "";

	_design = _product->cast<Design>();
	_units_manager = _design->fusionUnitsManager();
	json jresult;
	jresult["result"] = {  };
	for (Ptr<Parameter> parameter : _design->userParameters())
	{
		jresult["result"].push_back({ { "name", parameter->name() }, { "unit", parameter->unit() }, { "value", parameter->value() }, { "expression", parameter->expression() } });
	}
	return jresult;
}

class ThreadEventHandler : public CustomEventHandler
{
public:
	void notify(const Ptr<CustomEventArgs>& eventArgs) override
	{
		if (eventArgs)
		{
			std::string jsonString = getParameterJSON().dump();
			_palette->sendInfoToHTML("parameter-data", jsonString);
		}
	}
} onCustomEvent_;

void myThreadRun()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::string additionalInfo = std::to_string(rand());
	_app->fireCustomEvent(CUSTOM_EVENT_ID1, additionalInfo);
}

void onSendToHtml()
{
	std::thread myThread(myThreadRun);
	myThread.detach();
}

// Event handler for the palette close event.
class MyCloseEventHandler : public adsk::core::UserInterfaceGeneralEventHandler
{
public:
	void notify(const Ptr<UserInterfaceGeneralEventArgs>& eventArgs) override
	{
	}
} onClose_;

// Event handler for the palette HTML event.
class MyHTMLEventHandler : public adsk::core::HTMLEventHandler
{
public:
	void notify(const Ptr<HTMLEventArgs>& eventArgs) override
	{
		std::string action = eventArgs->action();
		_app = Application::get();
		if (!_app)
			return;

		_product = _app->activeProduct();
		if (!_product)
			return;

		_design = _app->activeProduct();
		if (!_product)
			return;

		_design = _product->cast<Design>();
		_units_manager = _design->fusionUnitsManager();

		if (action == "send")
		{
			_parameters = _design->userParameters();

			json obj = json::parse(eventArgs->data());
			for (auto item : obj["result"]) {
				auto expression = adsk::core::ValueInput::createByString(item["expression"]);
				auto p = _parameters->itemByName(item["name"]);
				p->expression(item["expression"]);
				p->value(item["value"]);
			}
		}
		if (action == "update-parameters")
		{
			onSendToHtml();
		}
	}
} onHTMLEvent_;

// Event handler for the commandExecuted event to show the palette.
class ShowPaletteCommandExecuteHandler : public adsk::core::CommandEventHandler
{
public:
	void notify(const Ptr<CommandEventArgs>& eventArgs) override
	{
		// Create a palette
		Ptr<Palettes> palettes = _ui->palettes();
		if (!palettes)
			return;
		_palette = palettes->itemById(PALLET_ID);
		if (!_palette)
		{
			_palette = palettes->add(PALLET_ID, PALLET_TITLE, HTML_FILE, true, true, true, PALLET_WIDTH, PALLET_HEIGHT);
			if (!_palette)
				return;

			// Dock the palette to the right side of Fusion window.
			_palette->dockingState(PaletteDockStateRight);
			_palette->dockingOption(adsk::core::PaletteDockingOptions::PaletteDockOptionsToVerticalOnly);

			// Add handler to HTMLEvent of the palette
			Ptr<HTMLEvent> htmlEvent = _palette->incomingFromHTML();
			if (!htmlEvent)
				return;

			htmlEvent->add(&onHTMLEvent_);

			// Add handler to CloseEvent of the palette
			Ptr<UserInterfaceGeneralEvent> closeEvent = _palette->closed();
			if (!closeEvent)
				return;
			closeEvent->add(&onClose_);
		}
		else
			_palette->isVisible(true);
		onSendToHtml();
	}
} onShowPaletteCommandExecuted_;

// Event handler for the commandCreated event.
class ShowPaletteCommandCreatedHandler : public adsk::core::CommandCreatedEventHandler
{
public:
	void notify(const Ptr<CommandCreatedEventArgs>& eventArgs) override
	{
		Ptr<Command> command = eventArgs->command();
		if (!command)
			return;
		Ptr<CommandEvent> exec = command->execute();
		if (!exec)
			return;
		exec->add(&onShowPaletteCommandExecuted_);
	}
} onShowPaletteCommandCreated_;

extern "C" XI_EXPORT bool run(const char* context)
{
	_app = Application::get();
	if (!_app)
		return false;

	_ui = _app->userInterface();
	if (!_ui)
		return false;

	Ptr<CommandDefinitions> commandDefinitions = _ui->commandDefinitions();
	if (!commandDefinitions)
		return false;

	// Add a command that displays the panel.
	Ptr<CommandDefinition> showPaletteCmdDef = commandDefinitions->itemById(TOOLBAR_COMMAND_ID);
	if (!showPaletteCmdDef) {
		showPaletteCmdDef = commandDefinitions->addButtonDefinition(TOOLBAR_COMMAND_ID, TOOLBAR_COMMAND_TITLE, TOOLBAR_COMMAND_DESCRIPTION, "");
		if (!showPaletteCmdDef)
			return false;

		// Connect to Command Created event.
		Ptr<CommandCreatedEvent> created = showPaletteCmdDef->commandCreated();
		if (!created)
			return false;
		created->add(&onShowPaletteCommandCreated_);
	}

	// Add the command to toolbar.
	Ptr<Workspaces> workspaces = _ui->workspaces();
	if (!workspaces)
		return false;

	Ptr<ToolbarPanelList> panels = _ui->allToolbarPanels();
	if (!panels)
		return false;

	Ptr<ToolbarPanel> panel = panels->itemById(TOOLBAR_PANEL);
	if (!panel)
		return false;

	Ptr<ToolbarControls> controls = panel->controls();
	if (!controls)
		return false;

	Ptr<ToolbarControl> ctrl = controls->itemById(TOOLBAR_COMMAND_ID);
	if (!ctrl)
		ctrl = controls->addCommand(showPaletteCmdDef);

	customEvent = _app->registerCustomEvent(CUSTOM_EVENT_ID1);
	if (!customEvent)
		return false;
	customEvent->add(&onCustomEvent_);

	return true;
}

extern "C" XI_EXPORT bool stop(const char* context)
{
	if (_ui)
	{
		// Delete the palette created by this add-in.
		Ptr<Palettes> palettes = _ui->palettes();
		if (!palettes)
			return false;

		Ptr<Palette> palette = palettes->itemById(PALLET_ID);
		if (palette)
			palette->deleteMe();

		// Delete controls and associated command definitions
		Ptr<ToolbarPanelList> panels = _ui->allToolbarPanels();
		if (!panels)
			return false;

		Ptr<ToolbarPanel> panel = panels->itemById(TOOLBAR_PANEL);
		if (!panel)
			return false;

		Ptr<ToolbarControls> controls = panel->controls();
		if (!controls)
			return false;

		Ptr<ToolbarControl> ctrl = controls->itemById(TOOLBAR_COMMAND_ID);
		if (ctrl)
			ctrl->deleteMe();

		Ptr<CommandDefinitions> commandDefinitions = _ui->commandDefinitions();
		if (!commandDefinitions)
			return false;

		Ptr<CommandDefinition> cmdDef = commandDefinitions->itemById(TOOLBAR_COMMAND_ID);
		if (cmdDef)
			cmdDef->deleteMe();

		customEvent->remove(&onCustomEvent_);
		_app->unregisterCustomEvent(CUSTOM_EVENT_ID1);
		_ui = nullptr;
	}

	return true;
}

#ifdef XI_WIN

#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hmodule, DWORD reason, LPVOID reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

#endif // XI_WIN