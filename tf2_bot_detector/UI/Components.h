#pragma once

#include <mh/error/expected.hpp>

#include <memory>

namespace tf2_bot_detector
{
	class IModeratorLogic;
	class IPlayer;
	class ITexture;
	class IWorldState;
	struct PlayerMarks;
	class Settings;
	enum class TeamShareResult;
}

namespace tf2_bot_detector::UI
{
	struct ColorPicker
	{
		const char* m_Name;
		std::array<float, 4>& m_Color;
	};
	[[nodiscard]] bool DrawColorPickers(const char* id, const std::initializer_list<ColorPicker>& pickers);
	[[nodiscard]] bool DrawColorPicker(const char* name, std::array<float, 4>& color);

	void DrawTeamStats(const IWorldState& world);

	void DrawScoreboard(IWorldState& world, IModeratorLogic& modLogic);

	mh::expected<std::shared_ptr<ITexture>> TryGetAvatarTexture(IPlayer& player);
	void DrawPlayerTooltip(const IModeratorLogic& modLogic, IPlayer& player);
	void DrawPlayerTooltip(IPlayer& player, TeamShareResult teamShareResult, const PlayerMarks& playerAttribs);
}
