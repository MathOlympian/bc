#include "IniFile.hpp"

#include <fstream> //for ini loading
#include <string> //for ini loading
#include <boost/algorithm/string.hpp> //for ini loading
#include <boost/lexical_cast.hpp> //for ini loading

// Irrlicht Namespaces
using namespace irr;

//Utility functions
namespace IniFile
{
    std::string enumerate1(std::string commandName, irr::u32 number)
    //Build up a command in the format 'commandName(#)'
    {
        std::string ans = commandName;
        ans.append("(");
        ans.append(boost::lexical_cast<std::string>(number));
        ans.append(")");

        return ans;
    }

    std::string enumerate2(std::string commandName, irr::u32 number1, irr::u32 number2)
    //Build up a command in the format 'commandName(#,#)'
    {
        std::string ans = commandName;
        ans.append("(");
        ans.append(boost::lexical_cast<std::string>(number1));
        ans.append(",");
        ans.append(boost::lexical_cast<std::string>(number2));
        ans.append(")");

        return ans;
    }

    std::string trim(const std::string& str, const std::string& trimChr=" ")
    {
        const std::size_t strBegin = str.find_first_not_of(trimChr);
        if (strBegin == std::string::npos)
            return ""; // no content
        const std::size_t strEnd = str.find_last_not_of(trimChr);
        const std::size_t strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }

    std::string iniFileToString(std::string fileName, std::string command)
    {
        std::ifstream file (fileName.c_str());
        std::string valuePart = "";
        if (file.is_open())
        {
            std::string line;
            while ( std::getline (file,line) )
            {
                std::string lowerLine(line); //Duplicate the line so we can make it lowercase, without affecting the result
                boost::to_lower(lowerLine);
                boost::to_lower(command); //Make the command lowercase, so we can check this without case sensitivity

                //Look for the command and the '=' sign in the line
                std::size_t commandFound = lowerLine.find(command);
                std::size_t equalsFound = lowerLine.find("=");

                //If both found, with equals after the command
                if (commandFound!=std::string::npos && equalsFound!=std::string::npos && equalsFound>commandFound)
                {
                    //Check that the part before the '=' exactly matches the command
                    if (trim(lowerLine.substr(0,equalsFound))==trim(command))
                    {
                        //get the value
                        try {
                            valuePart = line.substr(equalsFound+1,std::string::npos);//from equals to end, not including the equals
                        }
                        catch (const std::out_of_range& oor) {
                            std::cerr << "Could not get value for: " << command << " " << oor.what() << '\n';
                        }
                    }
                }

            }
            file.close();
        }
        else std::cout << "Unable to open file " << fileName << std::endl;

        //trim whitespace and " characters if present
        valuePart = trim(valuePart);
        valuePart = trim(valuePart,"\"");
        return valuePart;
    }

    //Load unsigned integer from an ini file
    u32 iniFileTou32(std::string fileName, std::string command)
    {
        u32 result = 0;
        std::string valueString = iniFileToString(fileName, command); //Get the value as a string
        result = core::strtoul10(valueString.c_str()); //Convert this into an unsigned int
        return result;
    }

    //Load float from an ini file
    f32 iniFileTof32(std::string fileName, std::string command)
    {
        f32 result = 0;
        std::string valueString = iniFileToString(fileName, command); //Get the value as a string
        result = core::fast_atof(valueString.c_str()); //Convert this into a float
        return result;
    }

}

