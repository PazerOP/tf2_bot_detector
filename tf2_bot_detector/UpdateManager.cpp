#include "UpdateManager.h"
#include "Config/Settings.h"
#include "Networking/GithubAPI.h"
#include "Networking/HTTPClient.h"
#include "Platform/Platform.h"
#include "Log.h"
#include "ReleaseChannel.h"

#include <mh/algorithm/multi_compare.hpp>
#include <mh/future.hpp>
#include <mh/types/disable_copy_move.hpp>

#include <variant>

using namespace tf2_bot_detector;

namespace
{
	class BaseUpdateCheck
	{
	public:
		BaseUpdateCheck(ReleaseChannel rc, const HTTPClient& client);
		virtual ~BaseUpdateCheck() = default;

		virtual void Update() = 0;
		virtual UpdateStatus GetUpdateStatus() const = 0;
		const IAvailableUpdate* GetAvailableUpdate() const;

	protected:
		virtual bool CanSelfUpdate() const = 0;
		virtual void BeginSelfUpdateImpl() const = 0;
		const HTTPClient& GetHTTPClient() const { return *m_Client; }

		struct AvailableUpdate final : public IAvailableUpdate
		{
			AvailableUpdate(BaseUpdateCheck& parent);

			ReleaseChannel m_ReleaseChannel{};
			ReleaseChannel GetReleaseChannel() const override final { return m_ReleaseChannel; }

			Version m_Version{};
			Version GetVersion() const override final { return m_Version; }

			bool CanSelfUpdate() const override { return m_Parent.CanSelfUpdate(); }
			void BeginSelfUpdate() const override final;

		private:
			BaseUpdateCheck& m_Parent;

		} m_AvailableUpdate;

	private:
		std::shared_ptr<const HTTPClient> m_Client;
	};

	BaseUpdateCheck::BaseUpdateCheck(ReleaseChannel rc, const HTTPClient& client) :
		m_AvailableUpdate(*this),
		m_Client(client.shared_from_this())
	{
		m_AvailableUpdate.m_ReleaseChannel = rc;
	}

	const IAvailableUpdate* BaseUpdateCheck::GetAvailableUpdate() const
	{
		return m_AvailableUpdate.m_Version > VERSION ? &m_AvailableUpdate : nullptr;
	}

	BaseUpdateCheck::AvailableUpdate::AvailableUpdate(BaseUpdateCheck& parent) :
		m_Parent(parent)
	{
	}

	void BaseUpdateCheck::AvailableUpdate::BeginSelfUpdate() const
	{
		if (!CanSelfUpdate())
			throw std::logic_error("BeginSelfUpdate() called when CanSelfUpdate() returned false");

		return m_Parent.BeginSelfUpdateImpl();
	}
}

namespace
{
	class PlatformUpdateCheck final : public BaseUpdateCheck
	{
	public:
		PlatformUpdateCheck(ReleaseChannel rc, const HTTPClient& client);

		void Update() override;
		UpdateStatus GetUpdateStatus() const override;

	protected:
		bool CanSelfUpdate() const override { return true; }
		void BeginSelfUpdateImpl() const override;

	private:
		std::variant<std::monostate, std::optional<Version>, std::exception_ptr> m_IsPlatformUpdateAvailable;
		std::future<std::optional<Version>> m_CheckForPlatformUpdateTask;
	};

	PlatformUpdateCheck::PlatformUpdateCheck(ReleaseChannel rc, const HTTPClient& client) :
		BaseUpdateCheck(rc, client)
	{
		m_CheckForPlatformUpdateTask = Platform::CheckForPlatformUpdate(rc, client);
	}

	void PlatformUpdateCheck::Update()
	{
		if (mh::is_future_ready(m_CheckForPlatformUpdateTask))
		{
			try
			{
				auto version = m_CheckForPlatformUpdateTask.get();
				m_IsPlatformUpdateAvailable = version;

				if (version)
					m_AvailableUpdate.m_Version = *version;
			}
			catch (const std::exception& e)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), e,
					"Failed to retrieve result of platform update availability check");
				m_IsPlatformUpdateAvailable = std::current_exception();
			}
		}
	}

	UpdateStatus PlatformUpdateCheck::GetUpdateStatus() const
	{
		if (std::holds_alternative<std::monostate>(m_IsPlatformUpdateAvailable))
			return UpdateStatus::Checking;
		else if (const auto* value = std::get_if<std::optional<Version>>(&m_IsPlatformUpdateAvailable))
			return *value ? UpdateStatus::UpdateAvailable : UpdateStatus::UpToDate;
		else if (std::holds_alternative<std::exception_ptr>(m_IsPlatformUpdateAvailable))
			return UpdateStatus::CheckFailed;
		else
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "Unknown variant index {}", m_IsPlatformUpdateAvailable.index());
			return UpdateStatus::CheckFailed;
		}
	}

	void PlatformUpdateCheck::BeginSelfUpdateImpl() const
	{
		auto update = GetAvailableUpdate();
		if (!update)
		{
			LogError(MH_SOURCE_LOCATION_CURRENT(), "BeginSelfUpdateImpl() called when GetAvailableUpdate() returned nullptr");
			return;
		}

		Platform::BeginPlatformUpdate(update->GetReleaseChannel(), GetHTTPClient());
	}
}

namespace
{
	class PortableUpdateCheck final : public BaseUpdateCheck
	{
	public:
		PortableUpdateCheck(ReleaseChannel rc, const HTTPClient& client);

		void Update() override;
		UpdateStatus GetUpdateStatus() const override;

	protected:
		bool CanSelfUpdate() const override { return false; }
		void BeginSelfUpdateImpl() const override;

	private:
		std::variant<
				std::future<GithubAPI::NewVersionResult>,
				GithubAPI::NewVersionResult,
				std::exception_ptr>
			m_NewVersionResult;
	};

	PortableUpdateCheck::PortableUpdateCheck(ReleaseChannel rc, const HTTPClient& client) :
		BaseUpdateCheck(rc, client)
	{
		m_NewVersionResult = GithubAPI::CheckForNewVersion(client);
	}

	void PortableUpdateCheck::Update()
	{
		if (auto future = std::get_if<std::future<GithubAPI::NewVersionResult>>(&m_NewVersionResult);
			future && mh::is_future_ready(*future))
		{
			try
			{
				auto versionCheckResult = future->get();
				m_NewVersionResult = versionCheckResult;

				if (m_AvailableUpdate.m_ReleaseChannel == ReleaseChannel::Preview &&
					versionCheckResult.IsPreviewAvailable())
				{
					m_AvailableUpdate.m_Version = versionCheckResult.m_Preview->m_Version;
				}
				else if (mh::any_eq(m_AvailableUpdate.m_ReleaseChannel, ReleaseChannel::Preview, ReleaseChannel::Public) &&
					versionCheckResult.IsReleaseAvailable())
				{
					m_AvailableUpdate.m_Version = versionCheckResult.m_Stable->m_Version;
				}
			}
			catch (const std::exception& e)
			{
				LogException(MH_SOURCE_LOCATION_CURRENT(), e,
					"Failed to retrieve result of platform update availability check");
				m_NewVersionResult = std::current_exception();
			}
		}
	}

	UpdateStatus PortableUpdateCheck::GetUpdateStatus() const
	{
		if (std::holds_alternative<std::future<GithubAPI::NewVersionResult>>(m_NewVersionResult))
		{
			return UpdateStatus::Checking;
		}
		else if (std::holds_alternative<GithubAPI::NewVersionResult>(m_NewVersionResult))
		{
			return m_AvailableUpdate.m_Version > VERSION ? UpdateStatus::UpdateAvailable : UpdateStatus::UpToDate;
		}
		else if (std::holds_alternative<std::exception_ptr>(m_NewVersionResult))
		{
			return UpdateStatus::CheckFailed;
		}

		LogError(MH_SOURCE_LOCATION_CURRENT(), "{}", UpdateStatus::InternalError_UnknownVariantState);
		return UpdateStatus::InternalError_UnknownVariantState;
	}

	void PortableUpdateCheck::BeginSelfUpdateImpl() const
	{
		throw std::logic_error("Portable self-updating not currently supported");
	}
}

namespace
{
	class UpdateManager final : public IUpdateManager
	{
	public:
		UpdateManager(const Settings& settings);

		void Update() override;
		UpdateStatus GetUpdateStatus() const override;

		const IAvailableUpdate* GetAvailableUpdate() const override;

		void QueueUpdateCheck() override { m_IsUpdateQueued = true; }

	private:
		const Settings& m_Settings;

		bool m_IsUpdateQueued = true;
		bool m_IsInstalled;
		std::unique_ptr<BaseUpdateCheck> m_UpdateCheck;
	};

	UpdateManager::UpdateManager(const Settings& settings) :
		m_Settings(settings),
		m_IsInstalled(Platform::IsInstalled())
	{
	}

	void UpdateManager::Update()
	{
		if (m_IsUpdateQueued)
		{
			auto client = m_Settings.GetHTTPClient();
			if (client && (m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::None) != ReleaseChannel::None))
			{
				if (m_IsInstalled)
					m_UpdateCheck = std::make_unique<PlatformUpdateCheck>(*m_Settings.m_ReleaseChannel, *client);
				else
					m_UpdateCheck = std::make_unique<PortableUpdateCheck>(*m_Settings.m_ReleaseChannel, *client);

				m_IsUpdateQueued = false;
			}
		}

		if (m_UpdateCheck)
			m_UpdateCheck->Update();
	}

	UpdateStatus UpdateManager::GetUpdateStatus() const
	{
		if (m_IsUpdateQueued)
		{
			if (m_Settings.m_ReleaseChannel.value_or(ReleaseChannel::None) == ReleaseChannel::None)
				return UpdateStatus::UpdateCheckDisabled;
			else if (!m_Settings.GetHTTPClient())
				return UpdateStatus::InternetAccessDisabled;
			else
				return UpdateStatus::CheckQueued;
		}

		if (m_UpdateCheck)
			return m_UpdateCheck->GetUpdateStatus();

		return UpdateStatus::InternalError_UpdateCheckNull;
	}

	const IAvailableUpdate* UpdateManager::GetAvailableUpdate() const
	{
		if (m_UpdateCheck)
			return m_UpdateCheck->GetAvailableUpdate();

		return nullptr;
	}
}

std::unique_ptr<IUpdateManager> tf2_bot_detector::IUpdateManager::Create(const Settings& settings)
{
	return std::make_unique<UpdateManager>(settings);
}
