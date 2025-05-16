/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde_json::json;
use url::Url;
use viaduct::Request;

const DEFAULT_MARS_API_ENDPOINT: &str = "https://ads.mozilla.org/v1/";

pub trait MARSClient: Sync + Send {
    fn delete(&self, context_id: String) -> crate::Result<()>;
}

pub struct SimpleMARSClient {
    endpoint: String,
}

impl SimpleMARSClient {
    pub fn new() -> Self {
        Self {
            endpoint: DEFAULT_MARS_API_ENDPOINT.to_string(),
        }
    }

    #[allow(dead_code)]
    fn new_with_endpoint(endpoint: String) -> Self {
        Self { endpoint }
    }
}

impl MARSClient for SimpleMARSClient {
    fn delete(&self, context_id: String) -> crate::Result<()> {
        let delete_url = Url::parse(&format!("{}/delete_user", self.endpoint))?;

        let body = json!({
            "context_id": context_id,
        })
        .to_string();
        let request = Request::delete(delete_url).body(body);

        let _ = request.send()?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use mockito::mock;

    #[test]
    fn test_delete() {
        viaduct_reqwest::use_reqwest_backend();

        let expected_context_id = "some-fake-context-id";
        let expected_body = format!(r#"{{"context_id":"{}"}}"#, expected_context_id);

        let m = mock("DELETE", "/delete_user")
            .match_body(expected_body.as_str())
            .create();

        let client = SimpleMARSClient::new_with_endpoint(mockito::server_url());
        let _ = client.delete(expected_context_id.to_string());
        m.expect(1).assert();
    }
}
