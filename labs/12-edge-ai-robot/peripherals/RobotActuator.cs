//
// RobotActuator.cs - a hand-written Renode peripheral for lab 12.
//
// Models the robot's memory-mapped "motor controller" register block.
// The firmware writes the currently-commanded skill here; this model
// mirrors the values and logs each skill command to the Renode monitor
// so you can watch the hardware side of the plan execute.
//
// Register map (offsets from 0x90000000):
//   0x00 SKILL_ID   current skill (1=WALK … 6=DOCK)
//   0x04 PARAM0     velocity (cm/s) or turn angle (deg)
//   0x08 PARAM1     distance (cm) or duration (s)
//   0x0C STEP       index of this skill within the plan
//

using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Robot
{
    public class RobotActuator : IDoubleWordPeripheral, IKnownSize
    {
        private uint skillId;
        private uint param0;
        private uint param1;
        private uint step;

        public long Size => 0x1000;

        public uint ReadDoubleWord(long offset)
        {
            switch (offset)
            {
            case (long)Registers.SkillId: return skillId;
            case (long)Registers.Param0:  return param0;
            case (long)Registers.Param1:  return param1;
            case (long)Registers.Step:    return step;
            default:
                this.Log(LogLevel.Warning, "read from unmapped offset 0x{0:X}", offset);
                return 0;
            }
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            switch (offset)
            {
            case (long)Registers.SkillId:
                skillId = value;
                break;
            case (long)Registers.Param0:
                param0 = value;
                break;
            case (long)Registers.Param1:
                param1 = value;
                break;
            case (long)Registers.Step:
                step = value;
                LogSkill();
                break;
            default:
                this.Log(LogLevel.Warning, "write 0x{0:X} to unmapped offset 0x{1:X}", value, offset);
                break;
            }
        }

        public void Reset()
        {
            skillId = 0;
            param0 = 0;
            param1 = 0;
            step = 0;
        }

        private void LogSkill()
        {
            var name = SkillName(skillId);
            switch (skillId)
            {
            case 1: // WALK
                this.Log(LogLevel.Info,
                    "actuator step {0}: {1} vel={2} cm/s dist={3} cm",
                    step, name, (int)param0, param1);
                break;
            case 2: // TURN
                this.Log(LogLevel.Info,
                    "actuator step {0}: {1} angle={2} deg",
                    step, name, param0);
                break;
            case 3: // KNEEL
            case 4: // HOLD
                this.Log(LogLevel.Info,
                    "actuator step {0}: {1} duration={2} s",
                    step, name, param1);
                break;
            default:
                this.Log(LogLevel.Info,
                    "actuator step {0}: {1}",
                    step, name);
                break;
            }
        }

        private static string SkillName(uint id)
        {
            switch (id)
            {
            case 1: return "WALK";
            case 2: return "TURN";
            case 3: return "KNEEL";
            case 4: return "HOLD";
            case 5: return "STOP";
            case 6: return "DOCK";
            default: return "?";
            }
        }

        private enum Registers : long
        {
            SkillId = 0x00,
            Param0  = 0x04,
            Param1  = 0x08,
            Step    = 0x0C,
        }
    }
}
