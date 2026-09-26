// ===========================================================================
//  NoMenuCloseDodge  ——  修复 TK Dodge RE“用手柄 B 键关闭菜单后角色莫名翻滚”
//
//  复刻目标：下架的 Nexus mod 172881
//            "No Menu Close Dodge - TK Dodge Controller Fix"（作者 Rorban）
//            其公开描述的效果为：
//              · 检测 Tween / 物品栏 / 地图 / 日志 等菜单的关闭
//              · 关闭后约 0.30 秒内屏蔽移动输入
//              · 因此不会再因为“输入缓冲”误触发 TK Dodge 的翻滚
//              · 不改动画、不需要重新设置手柄键位、不修改 TK Dodge 本体
//            本插件用同样的思路实现（0.30 秒移动控制屏蔽，时长可配置）。
//
//  病因（依据 TK Dodge RE 公开源码 max-su-2019/TK_Dodge_RE）：
//    src/Hooks.cpp  -> SprintHandlerHook::ProcessButton()
//        冲刺键短按（a_event->HeldDuration() < Settings::SprintingPressDuration）
//        在“抬起(IsUp)”时会直接调用 TKRE::dodge()。
//        用手柄 B 关菜单时，这次按下的“按下”发生在菜单里，菜单一关，
//        “抬起”事件立刻到达 -> 被判定为短按 -> 触发闪避 -> 角色翻滚。
//    src/TKRE.cpp   -> canDodge()
//        要求 controlMap->IsMovementControlsEnabled() 为真，
//        并且 GetDodgeEvent() 需要 PlayerControls 的移动输入向量非零。
//        => 只要在菜单关闭后短时间内让“移动控制 = 关闭”，canDodge() 必然失败，
//           翻滚就不会发生。这正是原版 MOD“移动输入屏蔽 0.30 秒”能修好的原因。
//
//  依赖：SKSE64 + Address Library for SKSE Plugins（TK Dodge RE 本身也需要）
//  编译：见同目录 README.md / build.ps1
// ===========================================================================

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <charconv>
#include <chrono>
#include <fstream>
#include <string>
#include <string_view>

using namespace std::chrono_literals;

namespace
{
	// -----------------------------------------------------------------------
	// 设置：Data\SKSE\Plugins\NoMenuCloseDodge.ini
	//   [Main]
	//   BlockDurationMs = 300   ; 菜单关闭后屏蔽移动输入的时长（毫秒，50~5000）
	// 读不到就用默认值，不放这个 ini 也能用。
	// -----------------------------------------------------------------------
	std::string Trim(std::string a_str)
	{
		const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
		while (!a_str.empty() && isSpace(static_cast<unsigned char>(a_str.front()))) {
			a_str.erase(a_str.begin());
		}
		while (!a_str.empty() && isSpace(static_cast<unsigned char>(a_str.back()))) {
			a_str.pop_back();
		}
		return a_str;
	}

	std::uint32_t ReadBlockDurationMs()
	{
		constexpr std::uint32_t kDefault = 300;

		const char* candidates[] = {
			"Data/SKSE/Plugins/NoMenuCloseDodge.ini",
			"SKSE/Plugins/NoMenuCloseDodge.ini",
			"NoMenuCloseDodge.ini"
		};

		for (const auto* path : candidates) {
			std::ifstream file(path);
			if (!file.is_open()) {
				continue;
			}

			std::string line;
			while (std::getline(file, line)) {
				if (const auto comment = line.find_first_of(";#"); comment != std::string::npos) {
					line.erase(comment);
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				const auto key = Trim(line.substr(0, eq));
				const auto value = Trim(line.substr(eq + 1));
				if (key != "BlockDurationMs" && key != "iBlockDurationMs") {
					continue;
				}
				std::uint32_t parsed = 0;
				const auto* begin = value.data();
				const auto* end = begin + value.size();
				if (const auto result = std::from_chars(begin, end, parsed); result.ec == std::errc{} && parsed >= 50 && parsed <= 5000) {
					return parsed;
				}
			}
		}
		return kDefault;
	}

	[[nodiscard]] std::chrono::milliseconds BlockDuration()
	{
		static const auto ms = ReadBlockDurationMs();
		return std::chrono::milliseconds{ ms };
	}

	// 这些菜单关闭时不处理（它们不是玩家手动关掉的功能菜单，
	// 例如 HUD、读盘、主菜单、控制台；屏蔽移动没有意义或会造成异常）
	constexpr std::string_view kIgnoredMenus[] = {
		"HUD Menu"sv,
		"Cursor Menu"sv,
		"Fader Menu"sv,
		"Loading Menu"sv,
		"Main Menu"sv,
		"Console"sv,
		"Debug Text Menu"sv,
		"Tutorial Menu"sv
	};

	[[nodiscard]] bool IsIgnoredMenu(const char* a_name)
	{
		if (!a_name || !*a_name) {
			return true;
		}
		const std::string_view name{ a_name };
		for (const auto ignored : kIgnoredMenus) {
			if (name == ignored) {
				return true;
			}
		}
		return false;
	}

	// -----------------------------------------------------------------------
	// 移动输入屏蔽：菜单关闭时立刻关掉“移动”控制，过一会儿再恢复。
	// 立刻执行是关键：B 键的“抬起”事件总在菜单关闭之后才到达，
	// 此时 canDodge() 里的 IsMovementControlsEnabled() 已经是 false。
	// -----------------------------------------------------------------------
	class MovementGuard
	{
	public:
		static MovementGuard& GetSingleton()
		{
			static MovementGuard singleton;
			return singleton;
		}

		void Start()
		{
			_deadline = std::chrono::steady_clock::now() + BlockDuration();
			if (_running) {
				return;  // 已经在屏蔽中，只延长截止时间即可
			}
			_running = true;
			Apply();
			Schedule();
		}

	private:
		MovementGuard() = default;

		void Schedule()
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) {
				Finish();
				return;
			}
			tasks->AddTask([]() { MovementGuard::GetSingleton().Tick(); });
		}

		void Tick()
		{
			if (!_running) {
				return;
			}
			if (std::chrono::steady_clock::now() >= _deadline) {
				Finish();
				return;
			}
			// 菜单关闭时引擎自己也可能会恢复一次控制权，所以这里每帧再压一次
			Apply();
			Schedule();
		}

		void Apply()
		{
			auto* controlMap = RE::ControlMap::GetSingleton();
			if (controlMap && controlMap->IsMovementControlsEnabled()) {
				// 与 SKSE 的 Input.DisablePlayerControls(abMovement = true) 操作的是同一组开关；
				// TK Dodge RE 的 canDodge() 明确检查 IsMovementControlsEnabled()。
				controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, false, false);
				_weDisabled = true;
			}

			// 顺带清掉移动输入向量：TK Dodge RE 的 GetDodgeEvent() 靠它判断翻滚方向。
			// 即使别的地方放行了 dodge()，没有方向也只会走 defaultDodgeEvent
			// （TK Dodge RE.ini 里把 defaultDodgeEvent 设成 EndAnimatedCamera 时不会播放动画）。
			if (auto* playerControls = RE::PlayerControls::GetSingleton()) {
				playerControls->data.moveInputVec = RE::NiPoint2{ 0.0f, 0.0f };
				playerControls->data.prevMoveVec = RE::NiPoint2{ 0.0f, 0.0f };
			}
		}

		void Finish()
		{
			if (_weDisabled) {
				if (auto* controlMap = RE::ControlMap::GetSingleton()) {
					controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, true, false);
				}
				_weDisabled = false;
			}
			_running = false;
		}

		std::chrono::steady_clock::time_point _deadline{};
		bool                               _running{ false };
		bool                               _weDisabled{ false };
	};

	// -----------------------------------------------------------------------
	// 菜单开关事件
	// -----------------------------------------------------------------------
	class MenuCloseHandler final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuCloseHandler* GetSingleton()
		{
			static MenuCloseHandler singleton;
			return std::addressof(singleton);
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event || a_event->opening) {
				return RE::BSEventNotifyControl::kContinue;
			}
			if (IsIgnoredMenu(a_event->menuName.c_str())) {
				return RE::BSEventNotifyControl::kContinue;
			}

			SKSE::log::info("menu closed: {} -> block movement input for {} ms",
				a_event->menuName.c_str(), BlockDuration().count());
			MovementGuard::GetSingleton().Start();
			return RE::BSEventNotifyControl::kContinue;
		}

	private:
		MenuCloseHandler() = default;
	};

	void SetupLog()
	{
		auto logsFolder = SKSE::log::log_directory();
		if (!logsFolder) {
			return;
		}
		const auto logFilePath = *logsFolder / "NoMenuCloseDodge.log";

		auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFilePath.string(), true);
		auto logger = std::make_shared<spdlog::logger>("NoMenuCloseDodge", std::move(fileSink));
		logger->set_level(spdlog::level::info);
		logger->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(logger));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) {
			return;
		}

		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			{
				auto* ui = RE::UI::GetSingleton();
				if (!ui) {
					SKSE::log::error("UI singleton not found, menu sink not registered");
					return;
				}
				ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuCloseHandler::GetSingleton());
				SKSE::log::info("registered MenuOpenCloseEvent sink (block duration {} ms)", BlockDuration().count());
			}
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	SetupLog();
	SKSE::log::info("NoMenuCloseDodge v1.0.0 loading... (runtime {})", REL::Module::get().version().string());

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler)) {
		SKSE::log::error("failed to register SKSE messaging listener");
		return false;
	}

	SKSE::log::info("loaded ok");
	return true;
}
