# ewoc-iz2jffs.sh -- Easy Wifi Configurator (for iz2jffs) -- EWoC
. /mnt/ffs/etc/brand.cfg

#setfont /mnt/ffs/share/fonts/ter-112n.psf
setfont /mnt/ffs/share/fonts/lispm-8x14.psf

WPA_CONF="/mnt/ffs/etc/wpa_supplicant.conf"
WLAN=eth0
#WLAN=wlan0

currentssid=$(grep 'ssid="' ${WPA_CONF} |head -1 | sed -e 's/.*ssid="/  /' -e 's/"//')
dialog --backtitle "$backtitle" --title "Use current Wifi settings?" --yesno "SSID: $currentssid" 6 0
if [ $? -ne 0 ]; then 
  iwlist ${WLAN} scan | grep 'ESSID' | sed -e 's/.*ESSID:"\([^"]\+\)".*/  \1/' > /tmp/ap_list.txt

  echo "dialog --nocancel --backtitle \"$backtitle\" \\" > /tmp/choose_ap.sh
  echo "--title \"Choose SSID\" \\" >> /tmp/choose_ap.sh
  echo "--radiolist \"\" \\" >> /tmp/choose_ap.sh

  LINES=`wc -l < /tmp/ap_list.txt`
  LINES=$((${LINES}+1))
  echo "10 30 ${LINES} \\" >> /tmp/choose_ap.sh
  CNT=1
  for LINE in `cat /tmp/ap_list.txt`
  do
    echo "${CNT} $LINE off \\" >> /tmp/choose_ap.sh
    CNT=$((${CNT}+1))
  done
  echo "${CNT} NAMED\ SSID on 2>/tmp/ssidnumber.ans" >>/tmp/choose_ap.sh
    
    
  chmod 777 /tmp/choose_ap.sh
  . /tmp/choose_ap.sh
      
  CHOOSENSSID=`cat /tmp/ssidnumber.ans`
      
  if [ $CHOOSENSSID == $LINES ]; then
    dialog --nocancel --ok-label "Submit" \
           --backtitle "$backtitle" \
           --title "SSID" \
           --inputbox ""  8 30 2>/tmp/ssid.ans
  else
    cat /tmp/ap_list.txt | head -${CHOOSENSSID} |tail -1 | sed -e 's/^[ \t]*//' >/tmp/ssid.ans
  fi
  
  dialog --nocancel --backtitle "$backtitle" \
          --title "Encryption Method" \
          --radiolist "" \
          10 30 4 \
          1 "WPA/WPA2" on \
          2 "WEP (hex)" off \
          3 "WEP (ascii)" off \
          4 "None" off 2>/tmp/encryption.ans

  echo "ctrl_interface=/tmp/wpa_ctrl" > ${WPA_CONF}

  SSID=`cat /tmp/ssid.ans`
  ENCRYPTION=`cat /tmp/encryption.ans`

  case $ENCRYPTION in
    '1')
      dialog --nocancel --ok-label "Submit" \
             --backtitle "$backtitle" \
             --title "Passphrase" \
             --inputbox ""  8 30 2>/tmp/passphrase.ans
      PASSPHRASE=`cat /tmp/passphrase.ans`
      #wpa_passphrase $SSID $PASSPHRASE | grep -v "^#" | grep -v "#psk=" >> ${WPA_CONF}
      wpa_passphrase $SSID $PASSPHRASE >> ${WPA_CONF}
      cp ${WPA_CONF} /tmp
      ;;
    '2')
       dialog --nocancel --ok-label "Submit" \
              --backtitle "$backtitle" \
              --title "Passphrase" \
              --inputbox "WEP key (hex)"  8 30 2>/tmp/passphrase.ans
       PASSPHRASE=`cat /tmp/passphrase.ans`
       echo "network={
                   ssid=\"$SSID\"
                   key_mgmt=NONE
                   wep_key0=$PASSPHRASE
                   }" >> ${WPA_CONF}
       ;;
    '3')
      dialog --nocancel --ok-label "Submit" \
             --backtitle "$backtitle" \
             --title "Passphrase" \
             --inputbox "WEP key (ascii)"  8 30 2>/tmp/passphrase.ans
       PASSPHRASE=`cat /tmp/passphrase.ans`
       echo "network={
                   ssid=\"$SSID\"
                   key_mgmt=NONE
                   wep_key0=\"$PASSPHRASE\"
                   }" >> ${WPA_CONF}
       ;;
    '4')
      echo "network={
                   ssid=\"$SSID\"
                   key_mgmt=NONE
                   }" >> ${WPA_CONF}
      ;;
  esac
fi
killall wpa_supplicant 2>/dev/null
/mnt/ffs/wpa_supplicant/wpa_supplicant -B -i${WLAN} -c ${WPA_CONF} 2>/dev/null
cls
echo Obtaining an IP address ...
udhcpc -n |tee /tmp/udhcpc.log
grep "No lease, failing" /tmp/udhcpc.log
nolease=$?
if [ $nolease -eq 0  ] ;  then
  dialog --backtitle "$backtitle" --title "Wifi Error" --ok-label "Poweroff" \
         --extra-button --extra-label "Reboot" --cancel-label "Continue" \
         --yesno "" 0 0
  case $? in
  3)
    echo "Rebooting "
    reboot
    sleep 5
    ;;
  0)
    echo "Powering off "
    poweroff
    sleep 5
    ;;
  esac
else
  ntpdate 0.pool.ntp.org
fi

setfont /mnt/ffs/share/fonts/myfont.psf
