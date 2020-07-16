using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;

namespace stm32IAPDemo
{
    class Command
    {
        const byte FRAME_HEADER = 0xA5;

        readonly SerialPort serport = new SerialPort();

        public Command(string comPort, int baudRate)
        {
            serport.PortName = comPort;
            serport.BaudRate = baudRate;
            serport.StopBits = StopBits.One;
            serport.DataBits = 8;
            serport.Parity = Parity.None;
            serport.ReadTimeout = 1000;
        }

        private byte CalcCheckSum(byte[] data)
        {
            byte chksum = 0;
            for (int i = 0; i < data.Length; i++)
            {
                chksum = (byte)(chksum ^ data[i]);
            }
            chksum = (byte)((chksum + 0x01) & 0xff);
            return chksum;
        }

        private void SendAndGetBytes(byte[] transmitBuffer, out byte[] receiveData)
        {
            List<byte> receiveList = new List<byte>();
            try
            {
                serport.Open();
                byte chksum = CalcCheckSum(transmitBuffer.ToList().GetRange(1, transmitBuffer.Length - 2).ToArray());
                transmitBuffer[transmitBuffer.Length - 1] = chksum;
                serport.Write(transmitBuffer, 0, transmitBuffer.Length);

                // Waiting for frame header
                do
                {
                    receiveList.Add((byte)serport.ReadByte());
                } while (receiveList.Last() != FRAME_HEADER);

                byte length = (byte)serport.ReadByte();
                receiveList.Add(length);
                receiveData = new byte[length + 1];
                receiveData[0] = FRAME_HEADER;
                receiveData[1] = length;

                for (int i = 0; i < (length - 1); i++)
                {
                    receiveData[i + 2] = (byte)serport.ReadByte();
                    receiveList.Add(receiveData[i + 2]);
                }

                if (CalcCheckSum(receiveData.ToList().GetRange(1, receiveData.Length - 2).ToArray()) != receiveData.Last())
                    throw new Exception("checksum error");

                ushort transmitCmdID = BitConverter.ToUInt16(new byte[] { transmitBuffer[3], transmitBuffer[2] }, 0);
                ushort receiveCmdID = BitConverter.ToUInt16(new byte[] { receiveData[3], receiveData[2] }, 0);

                if (transmitCmdID != receiveCmdID)
                    throw new Exception("received cmd not equal to transmit cmd");

                byte status = receiveData[receiveData.Length - 2];
                receiveData = receiveData.ToList().GetRange(4, receiveData.Length - 6).ToArray();
                if (status == 0x00)
                    return;
                else
                    throw new Exception($"error code : 0x{status:X2}");
            }
            finally
            {
                serport.Close();
            }
        }

        private byte[] SendAndGetFrame(ushort cmd, byte[] senddata = null)
        {
            byte[] transmitBuffer;
            if (senddata == null)
            {
                transmitBuffer = new byte[5];
            }
            else
            {
                transmitBuffer = new byte[5 + senddata.Length];
            }
            transmitBuffer[0] = FRAME_HEADER;
            transmitBuffer[1] = (byte)(transmitBuffer.Length - 1);
            transmitBuffer[2] = BitConverter.GetBytes(cmd)[1];
            transmitBuffer[3] = BitConverter.GetBytes(cmd)[0];
            if (senddata != null)
            {
                for (int i = 0; i < senddata.Length; i++)
                {
                    transmitBuffer[4 + i] = senddata[i];
                }
            }

            byte[] receiveData;
            SendAndGetBytes(transmitBuffer, out receiveData);

            return receiveData;
        }

        public void SetTimeToNow()
        {
            DateTime now = DateTime.Now;
            SendAndGetFrame(0x0001, new byte[]
            {
                (byte)(now.Year % 100),
                (byte)now.Month,
                (byte)now.Day,
                (byte)now.Hour,
                (byte)now.Minute,
                (byte)now.Second
            });
        }

        public string ReadTime()
        {
            byte[] receiveData = SendAndGetFrame(0x0002);
            return string.Join("-", receiveData);
        }

        public string GetInfo()
        {
            byte[] receiveData = SendAndGetFrame(0x0003);
            return Encoding.ASCII.GetString(receiveData);
        }

        public uint GetTemperatureADC()
        {
            byte[] receiveData = SendAndGetFrame(0x0007);
            return BitConverter.ToUInt32(receiveData.Reverse().ToArray(), 0);
        }

        public void UpdateFirmware(string updatefile)
        {
            SendAndGetFrame(0x0004, new byte[] { 0x01 });
            byte[] content = File.ReadAllBytes(updatefile);
            byte[] info = new byte[64];
            info[0] = BitConverter.GetBytes(content.Length)[3];
            info[1] = BitConverter.GetBytes(content.Length)[2];
            info[2] = BitConverter.GetBytes(content.Length)[1];
            info[3] = BitConverter.GetBytes(content.Length)[0];
            byte crc = CalcCheckSum(content);
            info[4] = crc;
            List<byte> sendbuff = new List<byte>();
            sendbuff.AddRange(info);
            sendbuff.AddRange(content);
            ushort packageNumber = 1;
            while (sendbuff.Count > 0)
            {
                byte[] temp;
                if (sendbuff.Count > 128)
                    temp = sendbuff.GetRange(0, 128).ToArray();
                else
                    temp = sendbuff.ToArray();
                sendbuff.RemoveRange(0, temp.Length);
                List<byte> temp1 = new List<byte>();
                temp1.Add(BitConverter.GetBytes(packageNumber)[1]);
                temp1.Add(BitConverter.GetBytes(packageNumber)[0]);
                temp1.AddRange(temp);
                SendAndGetFrame(0x0005, temp1.ToArray());
                packageNumber++;
            }
            SendAndGetFrame(0x0006);
        }
    }
}
