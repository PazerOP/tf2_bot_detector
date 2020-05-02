#include "ImGui_TF2BotDetector.h"

#include <stdexcept>

void ImGui::TextUnformatted(const std::string_view& text)
{
	return ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

void ImGui::TextColoredUnformatted(const ImVec4& col, const char* text, const char* text_end)
{
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	ImGui::TextUnformatted(text, text_end);
	ImGui::PopStyleColor();
}

void ImGui::TextColoredUnformatted(const ImVec4& col, const std::string_view& text)
{
	ImGui::TextColoredUnformatted(col, text.data(), text.data() + text.size());
}

void ImGui::AutoScrollBox(const char* ID, ImVec2 size, void(*contentsFn)(void* userData), void* userData)
{
	if (!contentsFn)
		throw std::invalid_argument("contentsFn cannot be nullptr");

	if (ImGui::BeginChild(ID, size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
	{
		const auto initialScrollY = ImGui::GetScrollY();
		const auto maxScrollY = ImGui::GetScrollMaxY();
		contentsFn(userData);

		auto storage = ImGui::GetStateStorage();
		static struct {} s_LastScrollPercentage; // Unique pointer
		const auto lastScrollPercentageID = ImGui::GetID(&s_LastScrollPercentage);

		static struct {} s_LastScrollMaxY;       // Unique pointer
		const auto lastScrollMaxYID = ImGui::GetID(&s_LastScrollMaxY);
		const auto lastScrollMaxY = storage->GetFloat(lastScrollMaxYID);

		const auto lastScrollPercentage = storage->GetFloat(lastScrollPercentageID, 1.0f);
		if (lastScrollPercentage >= 1.0f)
			ImGui::SetScrollY(ImGui::GetScrollMaxY());

		// Save the current scroll percentage
		{
			const auto cur = ImGui::GetScrollY();
			const auto max = ImGui::GetScrollMaxY();

			float curScrollPercentage = max == 0 ? 1.0f : cur / max;
			if (lastScrollPercentage >= 1.0f && max > lastScrollMaxY)
				curScrollPercentage = 1.0f;

			storage->SetFloat(lastScrollPercentageID, curScrollPercentage);
			storage->SetFloat(lastScrollMaxYID, max);
		}

#if 0
		ImGui::Text("Last scroll percentage: %f", storage->GetFloat(lastScrollPercentageID, NAN));
#endif
	}

	ImGui::EndChild();
}
