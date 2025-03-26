# SpyAI
Intelligent Malware that takes screenshots for entire monitors and exfiltrate them through Trusted Channel Slack to the C2 server that's using GPT-4 Vision to analyze them and construct daily activity — frame by frame 

#### Setup
For the Python server install those requirements using :
```
pip install slack_sdk requests openai pillow
```
And change in the Python script those fields:
```
SLACK_TOKEN = "<SLACK_TOKEN>"
SLACK_CHANNEL_ID = "<SLACK_CHANNEL_ID>"
CHECK_INTERVAL = <INTERVAL between each operation>  # seconds
openai.api_key = "<OPENAI_API_KEU>"
```
And change in the main.cpp:
```
const char* slackToken = "<SLACK_TOKEN>";
const char* slackChannel = "#<CHANNEL_NAME>";
...
Sleep(<Sleep Time in MS>); // In ms
```


#### Demo
[Watch demo video](https://streamable.com/qq78kt)
 
