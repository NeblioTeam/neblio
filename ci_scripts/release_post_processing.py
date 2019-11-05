import github3
import os
import requests
import hashlib
import time

gh = github3.login(token=os.environ['GITHUB_TOKEN'])

repo = gh.repository('NeblioTeam', 'neblio')

win_q_str = ''
mac_q_str = ''
lin_q_str = ''
rpi_q_str = ''
lin_d_str = ''
rpi_d_str = ''



def check_assets():
    print("Checking Release for Assets")
    body = []

    rel = repo.latest_release()

    assets = []
    for a in rel.assets():
        assets.append(a)

    for x in assets:
        global win_q_str
        if ("neblio-Qt---windows_x86" in x.name and len(win_q_str) == 0):
            x.sha256 = download_and_checksum(x.browser_download_url)
            if len(x.sha256) == 64:
                win_q_str = '| <img src="https://raw.githubusercontent.com/NeblioTeam/neblio/master/doc/img/windows-10-100.png" alt="Windows Icon by icons8 from https://icons8.com/" height="48px"/> | [Download ' + rel.tag_name + '<br/>For Windows](' + x.browser_download_url + ') | `' + x.sha256 + '` |'
                # print(win_q_str)

        global mac_q_str
        if ("neblio-Qt---macOS" in x.name and len(mac_q_str) == 0):
            x.sha256 = download_and_checksum(x.browser_download_url)
            if len(x.sha256) == 64:
                mac_q_str = '| <img src="https://raw.githubusercontent.com/NeblioTeam/neblio/master/doc/img/apple-100.png" alt="Apple Icon by icons8 from https://icons8.com/" height="48px"/> | [Download ' + rel.tag_name + '<br/>For macOS](' + x.browser_download_url + ') | `' + x.sha256 + '` |'
                # print(mac_q_str)

        global lin_q_str
        if ("neblio-Qt---ubuntu16.04" in x.name and len(lin_q_str) == 0):
            x.sha256 = download_and_checksum(x.browser_download_url)
            if len(x.sha256) == 64:
                lin_q_str = '| <img src="https://raw.githubusercontent.com/NeblioTeam/neblio/master/doc/img/linux-100.png" alt="Linux Icon by icons8 from https://icons8.com/" height="48px"/> | [Download ' + rel.tag_name + '<br/>For Linux](' + x.browser_download_url + ') | `' + x.sha256 + '` |'
                # print(lin_q_str)

        global rpi_q_str
        if ("neblio-Qt---RPi-raspbian" in x.name and len(rpi_q_str) == 0):
            x.sha256 = download_and_checksum(x.browser_download_url)
            if len(x.sha256) == 64:
                rpi_q_str = '| <img src="https://raw.githubusercontent.com/NeblioTeam/neblio/master/doc/img/raspberry-pi-100.png" alt="Raspberry Pi Icon by icons8 from https://icons8.com/" height="48px"/> | [Download ' + rel.tag_name + '<br/>For Raspberry Pi](' + x.browser_download_url + ') | `' + x.sha256 + '` |'
                # print(rpi_q_str)

        global lin_d_str
        if ("nebliod---ubuntu16.04" in x.name and len(lin_d_str) == 0):
            x.sha256 = download_and_checksum(x.browser_download_url)
            if len(x.sha256) == 64:
                lin_d_str = '| <img src="https://raw.githubusercontent.com/NeblioTeam/neblio/master/doc/img/linux-100.png" alt="Linux Icon by icons8 from https://icons8.com/" height="48px"/> | [Download ' + rel.tag_name + '<br/>For Linux](' + x.browser_download_url + ') | `' + x.sha256 + '` |'
                # print(lin_d_str)

        global rpi_d_str
        if ("nebliod---RPi-raspbian" in x.name and len(rpi_d_str) == 0):
            x.sha256 = download_and_checksum(x.browser_download_url)
            if len(x.sha256) == 64:
                rpi_d_str = '| <img src="https://raw.githubusercontent.com/NeblioTeam/neblio/master/doc/img/raspberry-pi-100.png" alt="Raspberry Pi Icon by icons8 from https://icons8.com/" height="48px"/> | [Download ' + rel.tag_name + '<br/>For Raspberry Pi](' + x.browser_download_url + ') | `' + x.sha256 + '` |'
                # print(rpi_d_str)

    # build our release body table
    body.append('## neblio-Qt (Wallet with User Interface)')
    body.append('| System | Download | Sha256 Checksum |')
    body.append('|:---:|:---:|:---|')
    if len(win_q_str) > 0:
        body.append(win_q_str)
    if len(mac_q_str) > 0:
        body.append(mac_q_str)
    if len(lin_q_str) > 0:
        body.append(lin_q_str)
    if len(rpi_q_str) > 0:
        body.append(rpi_q_str)
    body.append('')
    body.append('')
    body.append('## nebliod (Server Node. Command Line Only)')
    body.append('| System | Download | Sha256 Checksum |')
    body.append('|:---:|:---:|:---|')
    if len(lin_d_str) > 0:
        body.append(lin_d_str)
    if len(rpi_d_str) > 0:
        body.append(rpi_d_str)

    body_str = "\r\n".join(body)
    # print(body_str)
    new_rel_text = update_release_text(rel.body, body_str)
    status = rel.edit(body=new_rel_text)
    if (status):
        print('Successfully Updated Release')
    else:
        print('Error Updating Release. Starting Over.')
        win_q_str = ''
        mac_q_str = ''
        lin_q_str = ''
        rpi_q_str = ''
        lin_d_str = ''
        rpi_d_str = ''


def update_release_text(existing_body, new_body):
    existing_body_lines = existing_body.split('\r\n')
    # check if our release table already exists
    if ('## neblio-Qt (Wallet with User Interface)' in existing_body_lines and
        '## nebliod (Server Node. Command Line Only)' in existing_body_lines):
        # slice out everything after ## neblio-Qt (Wallet with User Interface)
        index = existing_body_lines.index('## neblio-Qt (Wallet with User Interface)')
        original_body = existing_body_lines[:index]
        original_body.append(new_body)
        return "\r\n".join(original_body)
    # else we have not added the table yet, let's add it
    else:
        existing_body_lines.append(new_body)
        return "\r\n".join(existing_body_lines)


def download_and_checksum(url):
    check = requests.head(url)
    if check.status_code > 399:
        print ("URL Invalid: " + url)
    # URL exists, download file
    print('Downloading URL to calculate checksum: ' + url)
    file_name = url.rsplit('/', 1)[1]
    r = requests.get(url, allow_redirects=True)
    if r.status_code > 399:
        print ("Download Failed: " + url)
    open(file_name, 'wb').write(r.content)
    # verify checksum
    sha256_hash = hashlib.sha256()
    with open(file_name, "rb") as f:
        # Read and update hash string value in blocks of 4K
        for byte_block in iter(lambda: f.read(4096),b""):
            sha256_hash.update(byte_block)
    sha256 = sha256_hash.hexdigest()
    return sha256

while(len(win_q_str) == 0 or
      len(mac_q_str) == 0 or
      len(lin_q_str) == 0 or
      len(rpi_q_str) == 0 or
      len(lin_d_str) == 0 or
      len(rpi_d_str) == 0):
    check_assets()
    time.sleep(30)
