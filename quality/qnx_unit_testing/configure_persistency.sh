#!/bin/sh

set -eu

PERSISTENT_PARTITION="/dev/hd1"
PERSISTENT_PARTITION_MOUNT_POINT="/qnx6fs_persistent"
PERSISTENT_ENCRYPTION_MOUNT_POINT=/persistent
QEMU=1

ENC_DOMAIN=3
ENC_TYPE=1

BackupEncrypted="true"

mount_qnx6fs_with_crypto() {
    echo "CONFIGURE_PERSISTENCY: mount_qnx6fs_with_crypto()"
    if mount | grep "$PERSISTENT_PARTITION_MOUNT_POINT"; then
        echo "CONFIGURE_PERSISTENCY: $PERSISTENT_PARTITION_MOUNT_POINT already mounted, returning"
    else
        echo "CONFIGURE_PERSISTENCY: $PERSISTENT_PARTITION_MOUNT_POINT not mounted, hence mounting"
        mount -t qnx6 -ocrypto=openssl "$PERSISTENT_PARTITION" "$PERSISTENT_PARTITION_MOUNT_POINT"
    fi
}

create_links(){
    echo "CONFIGURE_PERSISTENCY: create_links()"
    #create needed folders
    mkdir -p $PERSISTENT_PARTITION_MOUNT_POINT/persistent
    if [ ! -h $PERSISTENT_ENCRYPTION_MOUNT_POINT ]; then
        ln -s -P $PERSISTENT_PARTITION_MOUNT_POINT/persistent $PERSISTENT_ENCRYPTION_MOUNT_POINT
    fi
}

format_qnx6_then_mount() {
    echo "CONFIGURE_PERSISTENCY: format_qnx6_then_mount(): check if PER already mounted, if so unmount it before formatting!"
    if mount | grep "$PERSISTENT_PARTITION_MOUNT_POINT"; then
        echo "CONFIGURE_PERSISTENCY: $PERSISTENT_PARTITION_MOUNT_POINT already mounted, need to umount before formatting!"
        umount "$PERSISTENT_PARTITION_MOUNT_POINT"
    fi

    echo "CONFIGURE_PERSISTENCY: start formatting qnx6fs"
    if mkqnx6fs "$PERSISTENT_PARTITION" -q; then
        echo "CONFIGURE_PERSISTENCY: formatting done"
        if [ -h $PERSISTENT_ENCRYPTION_MOUNT_POINT ]; then
            echo "CONFIGURE_PERSISTENCY: remove $PERSISTENT_ENCRYPTION_MOUNT_POINT"
            rm $PERSISTENT_ENCRYPTION_MOUNT_POINT
        fi
        if ! mount_qnx6fs_with_crypto; then
            echo "CONFIGURE_PERSISTENCY: mounting failed, returning"
            return 1
        fi
    fi
    echo "CONFIGURE_PERSISTENCY: $PERSISTENT_PARTITION formatted and mounted now"
}

validate_encryption_status() {
    echo "CONFIGURE_PERSISTENCY: validate_encryption_status()"

    #mount in order to check if it is already encrypted
    if ! mount_qnx6fs_with_crypto; then
        echo "CONFIGURE_PERSISTENCY: mounting failed, returning"
        return 1
    fi

    #check the encryption status after the partition mounting
    query=$(fsencrypt -v -p "$PERSISTENT_PARTITION_MOUNT_POINT" -c query-all)
    domain_number=$(echo $query | awk -F' ' '{print $1}')
    domain_status=$(echo $query | awk -F' ' '{print $2}')
    echo "CONFIGURE_PERSISTENCY: domain_number: $domain_number"
    echo "CONFIGURE_PERSISTENCY: domain_status: $domain_status"
    if [[ -n "$domain_number" && -n "$domain_status" && "$domain_number" == "$ENC_DOMAIN" && ("$domain_status" == "UNLOCKED" || "$domain_status" == "LOCKED") ]]; then
        echo "CONFIGURE_PERSISTENCY: $PERSISTENT_PARTITION_MOUNT_POINT already encrypted"
        return 0
    else
        echo "CONFIGURE_PERSISTENCY: $PERSISTENT_PARTITION_MOUNT_POINT not yet encrypted"
        return 1
    fi
}

encrypt_persistent() {
    echo "CONFIGURE_PERSISTENCY: encrypt_persistent()"

    create_links

    key="B8428D25261B7E04F1311571BA0EA3EAEDA50ABC9AEE91B1355EF8065B75B227B8428D25261B7E04F1311571BA0EA3EAEDA50ABC9AEE91B1355EF8065B75B227"      #hard code key for Qemu

    echo "CONFIGURE_PERSISTENCY: Creating the encryption domain $ENC_DOMAIN"
    keybase64="$(echo -n $key | openssl base64 -A)"

    if fsencrypt -p "$PERSISTENT_PARTITION_MOUNT_POINT" -d $ENC_DOMAIN -c create -t $ENC_TYPE -k "#$keybase64"; then
        echo "CONFIGURE_PERSISTENCY: Encryption domain $ENC_DOMAIN has been created"
    else
        echo "CONFIGURE_PERSISTENCY: Something went wrong during encryption domain creation" >&2
        exit 1
    fi

    echo "CONFIGURE_PERSISTENCY: Setting the encryption domain $ENC_DOMAIN"
    if fsencrypt -p "$PERSISTENT_PARTITION_MOUNT_POINT/persistent" -d $ENC_DOMAIN -c set; then
        echo "CONFIGURE_PERSISTENCY: Encryption domain $ENC_DOMAIN has successfully been set"
    else
        echo "CONFIGURE_PERSISTENCY: Something went wrong during seting the encryption domain" >&2
        exit 1
    fi

    echo "CONFIGURE_PERSISTENCY: encrypt_persistent() done"
}

unlock_persistent() {
    echo "CONFIGURE_PERSISTENCY: unlock_persistent()"

    create_links

    #unlock the persistent if it is locked
    query=$(fsencrypt -v -p "$PERSISTENT_PARTITION_MOUNT_POINT" -c query-all)
    domain_number=$(echo $query | awk -F' ' '{print $1}')
    domain_status=$(echo $query | awk -F' ' '{print $2}')
    echo "CONFIGURE_PERSISTENCY: domain_number: $domain_number"
    echo "CONFIGURE_PERSISTENCY: domain_status: $domain_status"
    if [ -n "$domain_number" ] && [ -n "$domain_status" ] && [ "$domain_number" == "$ENC_DOMAIN" ] && [ "$domain_status" == "LOCKED" ] && [ "$domain_status" != "UNLOCKED" ]; then
        echo "CONFIGURE_PERSISTENCY: Unlocking the encryption domain $ENC_DOMAIN"

        key="B8428D25261B7E04F1311571BA0EA3EAEDA50ABC9AEE91B1355EF8065B75B227B8428D25261B7E04F1311571BA0EA3EAEDA50ABC9AEE91B1355EF8065B75B227"      #hard code some random key for Qemu
        echo "CONFIGURE_PERSISTENCY: key available, start unlocking encryption domain"

        echo "CONFIGURE_PERSISTENCY: Unlocking the encryption domain $ENC_DOMAIN"
        keybase64="$(echo -n $key | openssl base64 -A)"

        if fsencrypt -p "$PERSISTENT_PARTITION_MOUNT_POINT" -d $ENC_DOMAIN -c unlock -t $ENC_TYPE -k "#$keybase64"; then
            echo "CONFIGURE_PERSISTENCY: Encryption domain $ENC_DOMAIN has successfully been unlocked"
        else
            echo "CONFIGURE_PERSISTENCY: Something went wrong during unlocking the encryption domain" >&2
            return 1
        fi

    fi
    echo "CONFIGURE_PERSISTENCY: return from unlock_persistent()"
    return 0
}

format_mount_encrypt_restore(){
    echo "CONFIGURE_PERSISTENCY: format_mount_encrypt_restore()"
    format_qnx6_then_mount
    encrypt_persistent
}

main() {
    if validate_encryption_status; then
        echo "CONFIGURE_PERSISTENCY: persistent encrypted path"     #persistent encrypted
        if ! mount_qnx6fs_with_crypto; then
            format_mount_encrypt_restore
        else
            if ! unlock_persistent; then
                echo "CONFIGURE_PERSISTENCY: could not unlock persistent"
                format_mount_encrypt_restore
            fi
        fi
    else
        echo "CONFIGURE_PERSISTENCY: persistent unencrypted path"   #persistent unencrypted (also migration from Linux)
        format_mount_encrypt_restore
    fi

    echo "CONFIGURE_PERSISTENCY: Persistency configured successfully!"
}

main
