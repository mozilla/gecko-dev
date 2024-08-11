/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Guide models to respond with readable / parseable JSON-ish grammar. Allow
// some whitespace to avoid token resampling while ensuring escaped strings.
const JSON_GRAMMAR = `root ::= ws? "{" ws* key ( "," ws* key )* ws* "}" ws?
arr ::= "[" ws* str ( "," ws* str )* ws* "]" ws?
key ::= str ":" ws? val
str ::= ["] ( "\\\\" ["n] | [^\\\\"\\n] )+ ["] ws?
val ::= arr | str
ws  ::= [ \n]
`;

/**
 * Handle various kinds of ai/ml http inference APIs.
 */
export const HttpInference = {
  /**
   * GenAI prompt completion
   *
   * @param {object} config options for the request
   * @param {string} config.endpoint http completion API
   * @param {string} config.prompt to send
   * @param {string} config.bearer optional token for some endpoints
   * @param {string} config.model optional for some endpoints
   * @param {Function} config.onStream optional callback for streaming response
   * @param {object} context optional placeholder values
   * @returns {Promise<string>} response of the completion request
   */
  async completion(
    { bearer, endpoint, model, prompt, onStream },
    context = {}
  ) {
    let request, response;

    // Try to get JSON response if prompt includes "json"
    const expectJSON = prompt.search(/\bjson\b/i) >= 0;

    // Conditionally add prompt context if needed and allowed
    Object.entries(context).forEach(([key, val]) => {
      const placeholder = `%${key}%`;
      if (prompt.includes(placeholder)) {
        prompt = prompt.replace(placeholder, JSON.stringify(val));
      }
    });

    let streaming = !!onStream;

    // TODO: Pick a body format in a smarter way
    const body = {};
    if (endpoint.endsWith("/v1/chat/completions")) {
      body.messages = [{ content: prompt, role: "user" }];
      body.max_tokens = 1024;
      body.model = model;
      if (streaming) {
        body.stream = true;
      }
      if (expectJSON) {
        // TODO: Better deciding when to include grammar
        if (endpoint.includes("localhost")) {
          body.grammar = JSON_GRAMMAR;
        }
        body.response_format = { type: "json_object" };
      }
    } else if (endpoint.endsWith(":predict")) {
      body.instances = [{ content: prompt }];
      body.parameters = { maxOutputTokens: 1024 };
      streaming = false;
    } else if (endpoint.endsWith(":streamGenerateContent")) {
      body.contents = [{ parts: [{ text: prompt }], role: "user" }];
      body.generation_config = { maxOutputTokens: 1024 };
      // This endpoint doesn't do server-sent events format
      streaming = false;
    } else if (endpoint.endsWith("/completion")) {
      body.prompt = prompt;
      if (streaming) {
        body.stream = true;
      }
      if (expectJSON) {
        body.grammar = JSON_GRAMMAR;
      }
    } else {
      body.model = model;
      body.prompt = prompt;
      streaming = false;
    }

    const headers = {
      "Content-Type": "application/json",
    };
    if (bearer) {
      headers.Authorization = `Bearer ${bearer}`;
    }

    let ret = "";
    try {
      request = await fetch(endpoint, {
        body: JSON.stringify(body),
        headers,
        method: "POST",
      });

      if (request.status != 200) {
        throw await request.text();
      }

      if (streaming) {
        const reader = request.body.getReader();
        const decoder = new TextDecoder();
        // eslint-disable-next-line no-constant-condition
        while (true) {
          const { done, value } = await reader.read();
          if (done) {
            break;
          }

          // Read the JSON data of each server-sent event
          const lines = decoder
            .decode(value)
            .split("\n")
            .filter(l => l);
          for (const line of lines) {
            try {
              response = JSON.parse(line.replace(/^data: /, ""));
              const chunk =
                response.content ?? response.choices?.[0].delta.content;
              if (chunk?.length) {
                // Accumulate chunks for partial and final value
                ret += chunk;
                onStream(ret);
              }
            } catch (ex) {}
          }
        }
      } else {
        response = await request.json();
        ret =
          response.response ??
          response.content ??
          response.choices?.[0].message.content ??
          response.predictions?.[0].content ??
          response.map(r => r.candidates[0].content.parts[0].text).join("");

        // Some wrap JSON responses in code block
        if (expectJSON) {
          ret = ret.replace(/^\s*```\s*(json)?/i, "").replace(/```\s*$/, "");
        }
      }
    } catch (ex) {
      ret = [endpoint, request?.status, ex, JSON.stringify(response)].join(
        "\n\n"
      );
    }

    return ret;
  },
};
