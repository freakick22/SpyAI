import os
import time
import base64
import logging
import requests
from datetime import datetime
from PIL import Image
import openai

# === CONFIG ===
SLACK_TOKEN = "<SLACk_TOKEN>"
SLACK_CHANNEL_ID = "<SLACK_CHANNEL_ID>"
CHECK_INTERVAL = <INTERVAL between each operation>  # seconds
openai.api_key = "<OPENAI_API_KEU>"

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
last_file_id = None

def get_user_tag():
    domain = os.environ.get("USERDOMAIN", "UNKNOWN")
    user = os.environ.get("USERNAME", "UNKNOWN")
    return f"{domain}_{user}"

USER_TAG = get_user_tag()
BASE_DIR = os.path.join(os.getcwd(), USER_TAG)
os.makedirs(BASE_DIR, exist_ok=True)
GLOBAL_LOG_PATH = os.path.join(BASE_DIR, "global_summary.txt")

def convert_bmp_to_png(bmp_path):
    png_path = bmp_path.replace(".bmp", ".png")
    try:
        img = Image.open(bmp_path)
        img.save(png_path, "PNG")
        return png_path
    except Exception as e:
        logging.error(f"❌ Failed to convert BMP to PNG: {e}")
        return None

def get_latest_bitmap_from_slack():
    url = "https://slack.com/api/conversations.history"
    headers = {"Authorization": f"Bearer {SLACK_TOKEN}"}
    params = {"channel": SLACK_CHANNEL_ID, "limit": 10}

    response = requests.get(url, headers=headers, params=params)
    if response.status_code != 200:
        logging.error("Slack API error: %s", response.text)
        return None

    data = response.json()
    if not data.get("ok"):
        logging.error("Slack API error: %s", data.get("error"))
        return None

    for message in data["messages"]:
        for file in message.get("files", []):
            if file.get("filetype") == "bmp":
                return {
                    "id": file["id"],
                    "name": file["name"],
                    "url": file["url_private_download"]
                }

    return None

def download_bitmap(file_info):
    headers = {"Authorization": f"Bearer {SLACK_TOKEN}"}
    response = requests.get(file_info["url"], headers=headers)

    if response.status_code == 200:
        timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        screenshot_folder = os.path.join(BASE_DIR, timestamp)
        os.makedirs(screenshot_folder, exist_ok=True)
        local_path = os.path.join(screenshot_folder, file_info["name"])
        with open(local_path, "wb") as f:
            f.write(response.content)
        return local_path, screenshot_folder
    else:
        logging.error("Failed to download bitmap file from Slack.")
        return None, None

def analyze_screenshot(image_path):
    try:
        with open(image_path, "rb") as f:
            b64 = base64.b64encode(f.read()).decode()

        if not b64.strip():
            return "Invalid image."

        response = openai.ChatCompletion.create(
            model="gpt-4-turbo",
            messages=[
                {"role": "user", "content": [
                    {"type": "text", "text": "Describe what is happening in this screenshot."},
                    {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{b64}"}}
                ]}
            ],
            max_tokens=300
        )
        return response["choices"][0]["message"]["content"]
    except Exception as e:
        logging.error(f"❌ AI analysis failed: {e}")
        return "Error analyzing screenshot."

def main_loop():
    global last_file_id

    while True:
        logging.info("🔁 Checking for new BMP on Slack...")
        file_info = get_latest_bitmap_from_slack()

        if file_info and file_info["id"] != last_file_id:
            logging.info(f"🆕 New screenshot found: {file_info['name']}")
            bmp_path, screenshot_folder = download_bitmap(file_info)
            converted_path = convert_bmp_to_png(bmp_path)

            if converted_path:
                summary = analyze_screenshot(converted_path)

                # Save individual summary in screenshot folder
                with open(os.path.join(screenshot_folder, "summary.txt"), "w", encoding="utf-8") as f:
                    f.write(summary)

                # Append to global summary log
                with open(GLOBAL_LOG_PATH, "a", encoding="utf-8") as f:
                    f.write(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {file_info['name']}\n{summary}\n\n")

                logging.info("📝 Summary written.")
                last_file_id = file_info["id"]
        else:
            logging.info("😴 No new BMP yet. Sleeping...")

        time.sleep(CHECK_INTERVAL)

if __name__ == "__main__":
    try:
        main_loop()
    except KeyboardInterrupt:
        logging.info("🛑 Stopped.")
