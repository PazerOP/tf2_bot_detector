//Edited by The Immortal Nicholas Flamel with sanity checking by moeb for use with botdetector.tf
#base "botdetector_tf_hud_base/votehud.res"
"Resource/UI/VoteHud.res"
{
	// This is sent to the vote caller when they're not able to start the vote
	"CallVoteFailed"
	{
		"ControlName"	"EditablePanel"
		"fieldName"		"CallVoteFailed"
		"visible"		"0"
		"enabled"		"0"
		"xpos"			"9999"
		"ypos"			"9999"
		"wide"			"0"
		"tall"			"0"
		"FailedIcon"
		{
			"ControlName"	"ImagePanel"
			"fieldName"		"FailedIcon"
			"visible"		"0"
			"enabled"		"0"
			"xpos"			"9999"
			"ypos"			"9999"
			"wide"			"0"
			"tall"			"0"
		}

		"FailedTitle"
		{
			"ControlName"	"Label"
			"fieldName"		"FailedTitle"
			"visible"		"0"
			"enabled"		"0"
			"xpos"			"9999"
			"ypos"			"9999"
			"wide"			"0"
			"tall"			"0"
		}

		"FailedReason"
		{
			"ControlName"	"Label"
			"fieldName"		"FailedReason"
			"visible"		"0"
			"enabled"		"0"
			"xpos"			"9999"
			"ypos"			"9999"
			"wide"			"0"
			"tall"			"0"
		}
	}
}
