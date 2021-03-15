#include "AddonManagerPage.h"
#include "SetupFlow/ISetupFlowPage.h"
#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Util/JSONUtils.h"
#include "Filesystem.h"

#include <mh/algorithm/algorithm.hpp>
#include <mh/error/ensure.hpp>
#include <mh/io/filesystem_helpers.hpp>
#include <nlohmann/json.hpp>

#include <set>

#undef DrawState

using namespace tf2_bot_detector;

namespace
{
	struct Addon
	{
		std::filesystem::path m_Source;         // Path in ./tf2_addons/
		std::filesystem::path m_InstallTarget;  // Path inside of tf/addons/

		friend bool operator<(const Addon& lhs, const Addon& rhs)
		{
			return
				lhs.m_Source < rhs.m_Source &&
				lhs.m_InstallTarget < rhs.m_InstallTarget;
		}
	};

	static IAddonManager* s_AddonManager = nullptr;

	class AddonManagerPage final : public ISetupFlowPage, public IAddonManager
	{
	public:
		AddonManagerPage();
		~AddonManagerPage();

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		void Init(const InitState& is) override {}
		OnDrawResult OnDraw(const DrawState& ds) override;
		bool CanCommit() const override { return true; }
		void Commit(const CommitState& cs) override {}

		SetupFlowPage GetPage() const override { return SetupFlowPage::AddonManager; }

		mh::generator<std::filesystem::path> GetAllAvailableAddons() const override;
		mh::generator<std::filesystem::path> GetAllEnabledAddons(const Settings& settings) const override;
		bool IsAddonEnabled(const Settings& settings, const std::filesystem::path& addon) const override;
		void SetAddonEnabled(Settings& settings, const std::filesystem::path& addon, bool enabled) override;

	private:
		bool m_HasSetupAddons = false;

		static constexpr char ADDONS_LIST_FILENAME[] = "cfg/addons_list.json";

		std::set<Addon> BuildAddonsList(const Settings& settings) const;
		void SaveAddonsList(const std::set<Addon>& addons) const;
		std::set<Addon> LoadAddonsList() const;
		void RemoveAddonsList() const;
		void ConnectAddons(const std::set<Addon>& addons) const;
		void DisconnectAddons(const std::set<Addon>& addons) const;

		void ShutdownAddons(MH_SOURCE_LOCATION_AUTO(location)) const;
	};

	AddonManagerPage::AddonManagerPage()
	{
		mh_ensure(!std::exchange(s_AddonManager, this));
	}

	AddonManagerPage::~AddonManagerPage()
	{
		ShutdownAddons();
		mh_ensure(std::exchange(s_AddonManager, nullptr) == this);
	}

	void AddonManagerPage::ShutdownAddons(const mh::source_location& location) const try
	{
		DisconnectAddons(LoadAddonsList());
		RemoveAddonsList();
	}
	catch (...)
	{
		LogException(location);
	}

	AddonManagerPage::ValidateSettingsResult AddonManagerPage::ValidateSettings(const Settings& settings) const
	{
		return m_HasSetupAddons ? ValidateSettingsResult::Success : ValidateSettingsResult::TriggerOpen;
	}

	AddonManagerPage::OnDrawResult AddonManagerPage::OnDraw(const DrawState& ds)
	{
		ImGui::TextFmt("Setting up addons...");

		if (!m_HasSetupAddons)
		{
			try
			{
				ShutdownAddons();

				std::set<Addon> addons = BuildAddonsList(*mh_ensure(ds.m_Settings));
				{
					// In case there were any errors removing old addons, merge lists
					for (const Addon& addon : LoadAddonsList())
					{
						addons.insert(std::move(addon));
					}
				}
				SaveAddonsList(addons);
				ConnectAddons(addons);
			}
			catch (...)
			{
				LogException(); // TODO show this to the user and require them to press "continue" on error
			}

			m_HasSetupAddons = true;
		}

		return OnDrawResult::EndDrawing;
	}

	std::set<Addon> AddonManagerPage::BuildAddonsList(const Settings& settings) const try
	{
		std::set<Addon> addons;

		const auto tfDir = settings.GetTFDir();

		for (const std::filesystem::path& entry : GetAllEnabledAddons(settings))
		{
			Addon addon;
			addon.m_Source = entry;
			const auto baseInstallTarget = addon.m_InstallTarget = tfDir / "custom" / entry.filename();

			bool shouldSkip = false;
			if (std::filesystem::exists(addon.m_InstallTarget))
			{
				shouldSkip = true;
				for (size_t i = 2; i <= 10; i++)
				{
					if (std::filesystem::exists(addon.m_InstallTarget))
					{
						const auto originalInstallTarget = addon.m_InstallTarget;
						addon.m_InstallTarget = mh::replace_filename_keep_extension(baseInstallTarget,
							mh::format(MH_FMT_STRING("{}_{}"), mh::filename_without_extension(baseInstallTarget).string(), i));

						DebugLogWarning("{} already exists, trying {}...", originalInstallTarget, addon.m_InstallTarget);
					}
				}

				if (!std::filesystem::exists(addon.m_InstallTarget))
				{
					DebugLog("Selecting {}", addon.m_InstallTarget);
					shouldSkip = false;
				}
				else
				{
					LogError("Unable to find a filename for {} that didn't conflict with an addon that was already installed, skipping this particular addon.", addon.m_Source);
				}
			}

			if (!shouldSkip)
				addons.insert(std::move(addon));
		}

		return addons;
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void to_json(nlohmann::json& j, const Addon& addon)
	{
		j =
		{
			{ "source", addon.m_Source },
			{ "target", addon.m_InstallTarget },
		};
	}
	void from_json(const nlohmann::json& j, Addon& addon)
	{
		try_get_to_defaulted(j, addon.m_Source, "source");
		try_get_to_defaulted(j, addon.m_InstallTarget, "target");
	}

	void AddonManagerPage::SaveAddonsList(const std::set<Addon>& addons) const try
	{
		nlohmann::json j =
		{
			{ "addons", addons }
		};

		auto str = j.dump(1, '\t', true, nlohmann::detail::error_handler_t::ignore);

		DebugLog("Saving {}: {}", ADDONS_LIST_FILENAME, str);

		IFilesystem::Get().WriteFile(ADDONS_LIST_FILENAME, str, PathUsage::WriteLocal);
	}
	catch (...)
	{
		LogException();
		throw;
	}

	std::set<Addon> AddonManagerPage::LoadAddonsList() const try
	{
		std::string file;
		try
		{
			file = IFilesystem::Get().ReadFile(ADDONS_LIST_FILENAME);
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			if (e.code() == std::errc::no_such_file_or_directory)
				return {};
			else
				throw;
		}

		DebugLog("Loaded {}: {}", ADDONS_LIST_FILENAME, file);

		const nlohmann::json j = nlohmann::json::parse(file);

		return j.at("addons").get<std::set<Addon>>();
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void AddonManagerPage::ConnectAddons(const std::set<Addon>& addons) const try
	{
		for (const Addon& addon : addons)
		{
			try
			{
				if (std::filesystem::is_directory(addon.m_Source))
					std::filesystem::create_directory_symlink(addon.m_Source, addon.m_InstallTarget);
				else
					std::filesystem::create_symlink(addon.m_Source, addon.m_InstallTarget);
			}
			catch (const std::system_error& e)
			{
				if (e.code() == std::errc::permission_denied || e.code() == Platform::ErrorCodes::PRIVILEGE_NOT_HELD)
				{
					// Copy instead
					DebugLogWarning("Permission denied when trying to create symlink {} --> {}, copying instead", addon.m_InstallTarget, addon.m_Source);
					std::filesystem::copy(addon.m_Source, addon.m_InstallTarget,
						std::filesystem::copy_options::skip_existing | std::filesystem::copy_options::recursive);
				}
				else
				{
					throw;
				}
			}
		}
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void AddonManagerPage::DisconnectAddons(const std::set<Addon>& addons) const try
	{
		for (const Addon& addon : addons)
		{
			DebugLog("Deleting {}...", addon.m_InstallTarget);
			std::filesystem::remove_all(addon.m_InstallTarget);
		}
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void AddonManagerPage::RemoveAddonsList() const try
	{
		auto path = IFilesystem::Get().ResolvePath(ADDONS_LIST_FILENAME, PathUsage::WriteLocal);
		if (std::filesystem::exists(path))
		{
			DebugLog("Deleting {}...", path);
			std::filesystem::remove(path);
			DebugLog("Deleted {}.", path);
		}
		else
		{
			DebugLogWarning("{} does not exist, skipping delete", path);
		}
	}
	catch (...)
	{
		LogException();
		throw;
	}

	mh::generator<std::filesystem::path> AddonManagerPage::GetAllAvailableAddons() const
	{
		for (const std::filesystem::directory_entry& entry : IFilesystem::Get().IterateDir("tf2_addons", false))
		{
			if (!entry.is_regular_file() && !entry.is_directory())
				continue;

			co_yield entry.path();
		}
	}

	mh::generator<std::filesystem::path> AddonManagerPage::GetAllEnabledAddons(const Settings& settings) const
	{
		for (const std::filesystem::path& path : GetAllAvailableAddons())
		{
			if (!mh::contains(settings.m_Mods.m_DisabledList, path.filename().string()))
				co_yield path;
		}
	}

	bool AddonManagerPage::IsAddonEnabled(const Settings& settings, const std::filesystem::path& addon) const
	{
		return !mh::contains(settings.m_Mods.m_DisabledList, addon.filename().string());
	}

	void AddonManagerPage::SetAddonEnabled(Settings& settings, const std::filesystem::path& addon, bool enabled)
	{
		const std::string filename = addon.filename().string();

		mh::erase(settings.m_Mods.m_DisabledList, filename);

		if (!enabled)
			settings.m_Mods.m_DisabledList.push_back(filename);

		// TODO: Add/remove the mods directly here, rather than waiting for the tool to be restarted

		settings.SaveFile();
	}
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateAddonManagerPage()
	{
		return std::make_unique<AddonManagerPage>();
	}

	IAddonManager& IAddonManager::Get()
	{
		return *mh_ensure(s_AddonManager);
	}
}
