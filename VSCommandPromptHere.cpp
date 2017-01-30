/*
	Visual Studio Command Prompt Here
	Copyright (c) 2017, Adam Rehn
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/
#include <windows.h>
#include <direct.h>
#include <Shlobj.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <regex>
#include <vector>
using std::vector;
using std::string;

//The template for the contents of the installation .reg file
const char* installRegTemplate = R"(Windows Registry Editor Version 5.00

[HKEY_CLASSES_ROOT\Directory\shell\VSCommandPromptHere]
@="Visual Studio Command Prompt Here"
"Icon"="\"%%PATH_TO_VS%%devenv.exe\",0"

[HKEY_CLASSES_ROOT\Directory\shell\VSCommandPromptHere\command]
@="\"%%PATH_TO_EXE%%\\VSCommandPromptHere.exe\" \"%v\""

[HKEY_CLASSES_ROOT\Directory\Background\shell\VSCommandPromptHere]
@="Visual Studio Command Prompt Here"
"Icon"="\"%%PATH_TO_VS%%devenv.exe\",0"

[HKEY_CLASSES_ROOT\Directory\Background\shell\VSCommandPromptHere\command]
@="\"%%PATH_TO_EXE%%\\VSCommandPromptHere.exe\" \"%v\""
)";

//The contents of the uninstallation .reg file
const char* uninstallRegTemplate = R"(Windows Registry Editor Version 5.00

[-HKEY_CLASSES_ROOT\Directory\shell\VSCommandPromptHere]
[-HKEY_CLASSES_ROOT\Directory\Background\shell\VSCommandPromptHere]
)";

//Writes the contents of a file
bool putFileContents(const string& path, const string& data)
{
	std::ofstream outfile(path.c_str());
	if (outfile.is_open())
	{
		outfile.write(data.data(), data.length());
		outfile.close();
		return true;
	}
	
	return false;
}

//Adapted from a string search and replace function from <http://snipplr.com/view/1055/find-and-replace-one-string-with-another>
//and the code from the comment by "icstatic" that fixes the infinite loop bugs present in the original.
string strReplace(const string& find, const string& replace, string subject)
{
	size_t uPos        = 0;
	size_t uFindLen    = find.length();
	size_t uReplaceLen = replace.length();
	
	//If looking for an empty string, simply return the subject
	if (uFindLen == 0) {
		return subject;
	}
	
	//Loop through and find all instances
	while ((uPos = subject.find(find, uPos)) != string::npos)
	{
		subject.replace(uPos, uFindLen, replace);
		uPos += uReplaceLen;
	}
	
	return subject;
}

//Converts a wide Unicode string to an UTF-8 string
//(From <http://stackoverflow.com/a/3999597>)
std::string utf8_encode(const std::wstring &wstr)
{
	if( wstr.empty() ) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo( size_needed, 0 );
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

//Spawns a child process using ShellExecuteEx and waits for it to complete
bool spawnChildProcess(LPCTSTR lpOperation, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd)
{
	//Create a SHELLEXECUTEINFO struct to hold the arguments
	SHELLEXECUTEINFO ShExecInfo = {0};
	ShExecInfo.cbSize   = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask    = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd     = NULL;
	ShExecInfo.nShow    = nShowCmd;
	ShExecInfo.hInstApp = NULL;
	
	//Gather the arguments
	ShExecInfo.lpVerb       = lpOperation;
	ShExecInfo.lpFile       = lpFile;
	ShExecInfo.lpParameters = lpParameters;
	ShExecInfo.lpDirectory  = lpDirectory;
	
	//Perform the ShellExecuteEx and wait for the spawned process to complete
	if (ShellExecuteEx(&ShExecInfo))
	{
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		return true;
	}
	
	return false;
}

//Creates a .reg file with the specified path and contents, and prompts the user to add it to the registry
bool addToRegistry(const string& regfileName, const string& regfileContents)
{
	//Attempt to create the .reg file in the temp directory
	string regfilePath = string(getenv("TEMP")) + "/VSCommandPromptHere_" + regfileName;
	if (putFileContents(regfilePath, regfileContents))
	{
		//Prompt the user to add the contents of the .reg file to the registry
		spawnChildProcess("open", regfilePath.c_str(), "", _getcwd(nullptr, 0), SW_SHOW);
		
		//Remove the .reg file
		_unlink(regfilePath.c_str());
		
		//Notify the shell of updates so that any changes show up immediately
		SHChangeNotify(SHCNE_ASSOCCHANGED, 0, NULL, NULL);
		
		return true;
	}
	
	return false;
}

//Retrieves the list of environment variables for the current process
vector<string> listEnvironmentVariables()
{
	vector<string> varList;
	
	//Attempt to retrieve the environment block of the current process
	LPWCH envBlock = GetEnvironmentStringsW();
	if (envBlock == nullptr) {
		return varList;
	}
	
	//Iterate over each environment variable and add it to our list
	LPWSTR currVar = (LPWSTR)envBlock;
	while (*currVar)
	{
		varList.push_back(utf8_encode(std::wstring(currVar)));
		currVar += lstrlenW(currVar) + 1;
	}
	
	//Free the memory for the environment block
	FreeEnvironmentStringsW(envBlock);
	
	return varList;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	//Retrieve the list of environment variables
	vector<string> envVars = listEnvironmentVariables();
	
	//We will use a regex to identify each installed version of Visual Studio
	std::regex vsToolsVar("VS([0-9]+)COMNTOOLS=(.+)");
	std::smatch match;
	
	//When multiple Visual Studio versions are installed, we use the newest one
	string vsPath = "";
	int vsVersion = -1;
	
	//Iterate over the environment variables and locate any Visual Studio installations
	for (auto envVar : envVars)
	{
		if (std::regex_search(envVar, match, vsToolsVar))
		{
			//Extract the path and version number
			int version = strtol(match[1].str().c_str(), nullptr, 10);
			string path = match[2];
			
			//If the detected version is newer than the previous candidate, prefer this installation
			if (version != 0 && version > vsVersion)
			{
				vsVersion = version;
				vsPath = path;
			}
		}
	}
	
	//Verify that we detected an installation
	if (vsVersion != -1)
	{
		//Check if the user has requested installation or uninstallation
		string firstArg = ((__argc > 1) ? __argv[1] : "");
		if (firstArg == "--install")
		{
			//We are performing installation
			
			//Determine the location of the VS IDE, and escape it for inclusion in the .reg file
			string vsPathEscaped = strReplace("Common7\\Tools", "Common7\\IDE", vsPath);
			vsPathEscaped = strReplace("\\", "\\\\", vsPathEscaped);
			
			//Escape the current working directory for inclusion in the .reg file
			string cwd = _getcwd(nullptr, 0);
			string cwdEscaped = strReplace("\\", "\\\\", cwd);
			
			//Generate the .reg file and prompt the user to add it to the registry
			string regData = strReplace("%%PATH_TO_VS%%", vsPathEscaped, installRegTemplate);
			regData = strReplace("%%PATH_TO_EXE%%", cwdEscaped, regData);
			addToRegistry("install.reg", regData);
		}
		else if (firstArg == "--uninstall")
		{
			//We are performing uninstallation
			addToRegistry("uninstall.reg", uninstallRegTemplate);
		}
		else
		{
			//We are opening the VS command prompt
			
			//If a working directory was specified, use it
			string workingDir = ((!firstArg.empty()) ? firstArg : string(_getcwd(nullptr, 0)));
			
			//Spawn the Visual Studio Developer Command Prompt as a child process (supports VS2012 and newer)
			string params = "/k \"" + vsPath + "VsDevCmd.bat\"";
			spawnChildProcess("open", getenv("comspec"), params.c_str(), workingDir.c_str(), SW_SHOW);
		}
	}
	else
	{
		MessageBox(NULL, "No Visual Studio installation detected!", "Error", MB_ICONERROR);
		return 1;
	}
	
	return 0;
}
