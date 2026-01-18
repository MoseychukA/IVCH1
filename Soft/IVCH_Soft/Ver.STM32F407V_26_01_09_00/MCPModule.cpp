#include "MCPModule.h"
#include "ModuleController.h"
#include "Configuration_DEBUG.h"
#include "EEPROMSettingsModule.h"
//--------------------------------------------------------------------------------------------------------------------------------------
MCPModule::MCPModule() : AbstractModule("MCP")
{

}
//--------------------------------------------------------------------------------------------------------------------------------------
void MCPModule::Setup()
{
    // настройка модуля тут
}
//--------------------------------------------------------------------------------------------------------------------------------------
void MCPModule::Update()
{
    // обновление модуля тут 

}
//--------------------------------------------------------------------------------------------------------------------------------------
bool  MCPModule::ExecCommand(const Command& command, bool wantAnswer)
{
    UNUSED(wantAnswer);

    size_t argsCount = command.GetArgsCount();
    SysDebug debug = HardwareBinding->GetSysDebug();

    if (command.GetType() == ctGET)
    {
        if (argsCount < 2)
        {
            // params missed
            PublishSingleton = PARAMS_MISSED;
        }
        else
        {
            String cmd = command.GetArg(0);
            PublishSingleton.Flags.Status = true;
            PublishSingleton = cmd;

            byte mcpNumber = atoi(command.GetArg(0));
            byte mcpChannel = atoi(command.GetArg(1));

            int status = -1;

#if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0

#if defined MCP_DEBUG
            if (debug.MCP_DEBUG_K == HIGH)
            {
                Serial4.println("cmd == SPI");
            }

#endif   

            status = WORK_STATUS.MCP_SPI_PinRead(mcpNumber, mcpChannel);
#endif

#if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
            status = WORK_STATUS.MCP_I2C_PinRead(mcpNumber, mcpChannel);

#if defined MCP_DEBUG
            if (debug.MCP_DEBUG_K == HIGH)
            {
                Serial4.println("cmd == I2C");
            }

#endif   
#endif

#if defined(USE_PCF8575_EXTENDER) && COUNT_OF_PCF8575_EXTENDERS > 0
            status = WORK_STATUS.PCF8575_PinRead(mcpNumber, mcpChannel);

#if defined MCP_DEBUG
            if (debug.MCP_DEBUG_K == HIGH)
            {
                Serial4.println("cmd == PCF");
            }
#endif   
#endif

            PublishSingleton << PARAM_DELIMITER << mcpNumber << PARAM_DELIMITER << mcpChannel << PARAM_DELIMITER;
            if (status != -1)
            {
                PublishSingleton << (status == HIGH ? STATE_ON : STATE_OFF);
            }
            else
                PublishSingleton << status;
        }
    }
    else
        if (command.GetType() == ctSET)
        {
            if (argsCount < 4)
            {
                PublishSingleton = PARAMS_MISSED;
            }
            else
            {
                String cmd = command.GetArg(0);
                String operation = command.GetArg(0);
                byte mcpNumber = atoi(command.GetArg(1));
                byte mcpChannel = atoi(command.GetArg(2));
                String levelOrMode = command.GetArg(3);
                byte level = (levelOrMode == STATE_ON || levelOrMode == STATE_ON_ALT) ? LOW : HIGH;

                PublishSingleton.Flags.Status = true;
                PublishSingleton = cmd;
                PublishSingleton << PARAM_DELIMITER << operation;

                if (operation == F("MODE")) // CTSET=MCP|MODE|mcpNumber|mcpChannel|pinMode, for example  CTSET=MCP|MODE|0|7|OUT, CTSET=MCP|MODE|1|2|IN
                {
#if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0

                    byte mode = INPUT;
                    if (levelOrMode == F("OUT"))
                        mode = OUTPUT;

                    WORK_STATUS.MCP_SPI_PinMode(mcpNumber, mcpChannel, mode);
#if defined MCP_DEBUG
                    if (debug.MCP_DEBUG_K == HIGH)
                    {
                        Serial4.print("mcpNumber = ");
                        Serial4.println(mcpNumber);
                        Serial4.print("mcpChannel = ");
                        Serial4.println(mcpChannel);
                        Serial4.print("MCP_SPI_PinMode = ");
                        Serial4.println(mode);
                    }
#endif   

#endif    

#if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0

                    byte mode = INPUT;
                    if (levelOrMode == F("OUT"))
                        mode = OUTPUT;

                    WORK_STATUS.MCP_I2C_PinMode(mcpNumber, mcpChannel, mode);

#if defined MCP_DEBUG
                    if (debug.MCP_DEBUG_K == HIGH)
                    {
                        Serial4.print("mcpNumber = ");
                        Serial4.println(mcpNumber);
                        Serial4.print("mcpChannel = ");
                        Serial4.println(mcpChannel);
                        Serial4.print("MCP_I2C_PinMode = ");
                        Serial4.println(mode);
                    }
#endif   
#endif            

#if defined(USE_PCF8575_EXTENDER) && COUNT_OF_PCF8575_EXTENDERS > 0

                    byte mode = INPUT;
                    if (levelOrMode == F("OUT"))
                        mode = OUTPUT;

                    WORK_STATUS.PCF8575_PinMode(mcpNumber, mcpChannel, mode);

#if defined MCP_DEBUG
                    if (debug.MCP_DEBUG_K == HIGH)
                    {
                        Serial4.print("mcpNumber = ");
                        Serial4.println(mcpNumber);
                        Serial4.print("mcpChannel = ");
                        Serial4.println(mcpChannel);
                        Serial4.print("MCP_PCF_PinMode = ");
                        Serial4.println(mode);
                    }
#endif
#endif         

                }
                else // CTSET=MCP|WRITE|mcpNumber|mcpChannel|level
                {
#if defined(USE_MCP23S17_EXTENDER) && COUNT_OF_MCP23S17_EXTENDERS > 0
                    WORK_STATUS.MCP_SPI_PinMode(mcpNumber, mcpChannel, OUTPUT);
                    WORK_STATUS.MCP_SPI_PinWrite(mcpNumber, mcpChannel, level);

#if defined MCP_DEBUG
                    if (debug.MCP_DEBUG_K == HIGH)
                    {
                        Serial4.print("***mcpNumber ");
                        Serial4.println(mcpNumber);
                        Serial4.print(" pin ");
                        Serial4.print(mcpChannel);
                        Serial4.print(" level ");
                        Serial4.println(level);
                    }
#endif  

#endif

#if defined(USE_MCP23017_EXTENDER) && COUNT_OF_MCP23017_EXTENDERS > 0
                    WORK_STATUS.MCP_I2C_PinMode(mcpNumber, mcpChannel, OUTPUT);
                    WORK_STATUS.MCP_I2C_PinWrite(mcpNumber, mcpChannel, level);
#if defined MCP_DEBUG
                    if (debug.MCP_DEBUG_K == HIGH)
                    {
                        Serial4.print("***mcpNumber ");
                        Serial4.println(mcpNumber);
                        Serial4.print(" pin ");
                        Serial4.print(mcpChannel);
                        Serial4.print(" level ");
                        Serial4.println(level);
                    }
#endif  
#endif

#if defined(USE_PCF8575_EXTENDER) && COUNT_OF_PCF8575_EXTENDERS > 0
                    WORK_STATUS.PCF8575_PinMode(mcpNumber, mcpChannel, OUTPUT);
#if defined MCP_DEBUG
                    if (debug.MCP_DEBUG_K == HIGH)
                    {
                        Serial4.print("***mcpNumber ");
                        Serial4.println(mcpNumber);
                        Serial4.print(" pin ");
                        Serial4.print(mcpChannel);
                        Serial4.print(" level ");
                        Serial4.println(level);
                    }
#endif  
                    WORK_STATUS.PCF8575_PinWrite(mcpNumber, mcpChannel, level);
#endif


                }

                PublishSingleton << PARAM_DELIMITER << mcpNumber << PARAM_DELIMITER << mcpChannel << PARAM_DELIMITER << levelOrMode << PARAM_DELIMITER << REG_SUCC;
            }
        }
    // отвечаем на команду
    MainController->Publish(this, command);

    return PublishSingleton.Flags.Status;
}
//--------------------------------------------------------------------------------------------------------------------------------------


