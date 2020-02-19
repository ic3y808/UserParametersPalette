#include <Core/CoreAll.h>
#include <Fusion/FusionAll.h>
#include <thread>
#include "nlohmann/json.hpp"
#include "UserParameters.h"
#include "version.h"

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

void CollectVars() {
	_app = Application::get();
	if (!_app)
		return;
	_product = _app->activeProduct();
	_design = _product->cast<Design>();
	_units_manager = _design->fusionUnitsManager();
}

double ConvertToUnitValue(Ptr<Parameter> p) {
	std::string unit = p->unit();
	double origValue = p->value();

	if (unit == "mm") {
		return origValue * 10;
	}
	if (unit == "cm") {
		return origValue;
	}
}

double ConvertFromUnitValue(std::string unit, double val) {
	if (unit == "mm") {
		return val / 10;
	}
	if (unit == "cm") {
		return val;
	}
}

std::string ConvertExpression(std::string unit, double val) {
	return std::to_string(val) + " " + unit;
}

json getParameterJSON()
{
	CollectVars();
	json jresult;
	jresult["result"] = {  };
	for (Ptr<Parameter> parameter : _design->userParameters())
	{
		jresult["result"].push_back(
			{
				{ "name", parameter->name() },
				{ "unit", parameter->unit() },
				{ "value", ConvertToUnitValue(parameter) },
				{ "expression", parameter->expression() },
				{ "comment", parameter->comment() },
				{ "deletable", parameter->isDeletable() },
				{ "favorite", parameter->isFavorite() },
				{ "valid", parameter->isValid() },
				{ "dependent_count", parameter->dependentParameters()->count() },
			}
		);
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
			std::string result = _palette->sendInfoToHTML("parameter-data", jsonString);
		}
	}
} onCustomEvent_;

void threadRun()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::string additionalInfo = std::to_string(rand());
	_app->fireCustomEvent(CUSTOM_EVENT_ID1, additionalInfo);
}

void sendUpdateToUi()
{
	std::thread myThread(threadRun);
	myThread.detach();
}

class DocumentActivatedEventHandler : public adsk::core::DocumentEventHandler
{
public:
	void notify(const Ptr<DocumentEventArgs>& eventArgs) override
	{
		sendUpdateToUi();
	}
} _documentActivated;

class HTMLEventHandler : public adsk::core::HTMLEventHandler
{
public:
	void notify(const Ptr<HTMLEventArgs>& eventArgs) override
	{
		std::string action = eventArgs->action();
		CollectVars();

		if (action == "change-parameters")
		{
			_parameters = _design->userParameters();

			json obj = json::parse(eventArgs->data());
			for (auto item : obj["result"]) {
				
				Ptr<UserParameter> p = _parameters->itemByName(item["name"]);
				if (p != nullptr) {
					double val = item["value"];
					std::string unit = item["unit"];
					std::string expression = std::to_string(val);
					expression.append(" ");
					expression.append(unit);
					p->expression(expression);
				}
				else {
					Ptr<UserParameter> newParam = _parameters->add(item["name"], adsk::core::ValueInput::createByString(item["expression"]), item["unit"], "");
					sendUpdateToUi();
				}
			}
		}
		if (action == "update-parameters")
		{
			sendUpdateToUi();
		}
	}
} onHTMLEvent_;

class ShowPaletteCommandExecuteHandler : public adsk::core::CommandEventHandler
{
public:
	void notify(const Ptr<CommandEventArgs>& eventArgs) override
	{
		Ptr<Palettes> palettes = _ui->palettes();
		if (!palettes)
			return;
		_palette = palettes->itemById(PALLET_ID);
		if (!_palette)
		{
			_palette = palettes->add(PALLET_ID, PALLET_TITLE, HTML_FILE, true, false, true, PALLET_WIDTH, PALLET_HEIGHT);
			if (!_palette)
				return;

			_palette->dockingState(PaletteDockStateRight);
			_palette->dockingOption(adsk::core::PaletteDockingOptions::PaletteDockOptionsToVerticalOnly);

			Ptr<HTMLEvent> htmlEvent = _palette->incomingFromHTML();
			if (!htmlEvent)
				return;

			htmlEvent->add(&onHTMLEvent_);

		}
		else
			_palette->isVisible(true);
		sendUpdateToUi();
	}
} onShowPaletteCommandExecuted_;

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

	Ptr<DocumentEvent> documentActivatedEvent = _app->documentActivated();
	if (!documentActivatedEvent)
		return false;

	bool isOk = documentActivatedEvent->add(&_documentActivated);
	if (!isOk)
		return false;

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