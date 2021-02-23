#include "SetupFlow/ISetupFlowPage.h"
#include "Config/Settings.h"
#include "Platform/Platform.h"
#include "UI/ImGui_TF2BotDetector.h"
#include "Util/JSONUtils.h"
#include "Filesystem.h"

#include <mh/error/ensure.hpp>
#include <mh/io/filesystem_helpers.hpp>
#include <nlohmann/json.hpp>

#include <vector>

using namespace tf2_bot_detector;

namespace
{
	struct Addon
	{
		std::filesystem::path m_Source;         // Path in ./tf2_addons/
		std::filesystem::path m_InstallTarget;  // Path inside of tf/addons/
	};

	class AddonManagerPage final : public ISetupFlowPage
	{
	public:
		~AddonManagerPage();

		bool WantsSetupText() const override { return false; }
		bool WantsContinueButton() const override { return false; }

		ValidateSettingsResult ValidateSettings(const Settings& settings) const override;
		void Init(const InitState& is) override {}
		OnDrawResult OnDraw(const DrawState& ds) override;
		bool CanCommit() const override { return true; }
		void Commit(const CommitState& cs) override {}

		SetupFlowPage GetPage() const override { return SetupFlowPage::AddonManager; }

	private:
		bool m_HasSetupAddons = false;

		static constexpr char ADDONS_LIST_FILENAME[] = "cfg/addons_list.json";

		std::vector<Addon> BuildAddonsList(const Settings& settings) const;
		void SaveAddonsList(const std::vector<Addon>& addons) const;
		std::vector<Addon> LoadAddonsList() const;
		void RemoveAddonsList() const;
		void ConnectAddons(const std::vector<Addon>& addons) const;
		void DisconnectAddons(const std::vector<Addon>& addons) const;

		void ShutdownAddons(MH_SOURCE_LOCATION_AUTO(location)) const;
	};

	AddonManagerPage::~AddonManagerPage()
	{
		ShutdownAddons();
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

				const auto addons = BuildAddonsList(*mh_ensure(ds.m_Settings));
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

	std::vector<Addon> AddonManagerPage::BuildAddonsList(const Settings& settings) const try
	{
		std::vector<Addon> addons;

		const auto tfDir = settings.GetTFDir();

		for (const std::filesystem::directory_entry& entry : IFilesystem::Get().IterateDir("tf2_addons", false))
		{
			if (!entry.is_regular_file() && !entry.is_directory())
				continue;

			Addon addon;
			addon.m_Source = entry;
			addon.m_InstallTarget = tfDir / "custom" / entry.path().filename();

			bool shouldSkip = false;
			if (std::filesystem::exists(addon.m_InstallTarget))
			{
				shouldSkip = true;
				for (size_t i = 2; i <= 10; i++)
				{
					if (std::filesystem::exists(addon.m_InstallTarget))
					{
						const auto originalInstallTarget = addon.m_InstallTarget;
						addon.m_InstallTarget = mh::replace_filename_keep_extension(addon.m_InstallTarget,
							mh::format(MH_FMT_STRING("{}_{}"), mh::filename_without_extension(addon.m_InstallTarget).string(), i));

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
				addons.push_back(std::move(addon));
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

	void AddonManagerPage::SaveAddonsList(const std::vector<Addon>& addons) const try
	{
		nlohmann::json j =
		{
			{ "addons", addons }
		};

		auto str = j.dump(1, '\t', true, nlohmann::detail::error_handler_t::ignore);

		IFilesystem::Get().WriteFile(ADDONS_LIST_FILENAME, str, PathUsage::WriteLocal);
	}
	catch (...)
	{
		LogException();
		throw;
	}

	std::vector<Addon> AddonManagerPage::LoadAddonsList() const try
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

		const nlohmann::json j = nlohmann::json::parse(file);

		return j.at("addons").get<std::vector<Addon>>();
	}
	catch (...)
	{
		LogException();
		throw;
	}

	void AddonManagerPage::ConnectAddons(const std::vector<Addon>& addons) const try
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

	void AddonManagerPage::DisconnectAddons(const std::vector<Addon>& addons) const try
	{
		for (const Addon& addon : addons)
		{
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
}

namespace tf2_bot_detector
{
	std::unique_ptr<ISetupFlowPage> CreateAddonManagerPage()
	{
		return std::make_unique<AddonManagerPage>();
	}
}
