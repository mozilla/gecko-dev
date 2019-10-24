/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import ReactDOM from "react-dom";

export class DSImage extends React.PureComponent {
  constructor(props) {
    super(props);

    this.onOptimizedImageError = this.onOptimizedImageError.bind(this);
    this.onNonOptimizedImageError = this.onNonOptimizedImageError.bind(this);

    this.state = {
      isSeen: false,
      optimizedImageFailed: false,
      useTransition: false,
    };
  }

  onSeen(entries) {
    if (this.state) {
      const entry = entries.find(e => e.isIntersecting);

      if (entry) {
        if (this.props.optimize) {
          this.setState({
            // Thumbor doesn't handle subpixels and just errors out, so rounding...
            containerWidth: Math.round(entry.boundingClientRect.width),
            containerHeight: Math.round(entry.boundingClientRect.height),
          });
        }

        this.setState({
          isSeen: true,
        });

        // Stop observing since element has been seen
        this.observer.unobserve(ReactDOM.findDOMNode(this));
      }
    }
  }

  onIdleCallback() {
    if (!this.state.isSeen) {
      this.setState({
        useTransition: true,
      });
    }
  }

  reformatImageURL(url, width, height) {
    // Change the image URL to request a size tailored for the parent container width
    // Also: force JPEG, quality 60, no upscaling, no EXIF data
    // Uses Thumbor: https://thumbor.readthedocs.io/en/latest/usage.html
    return `https://img-getpocket.cdn.mozilla.net/${width}x${height}/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(
      url
    )}`;
  }

  componentDidMount() {
    this.idleCallbackId = this.props.windowObj.requestIdleCallback(
      this.onIdleCallback.bind(this)
    );
    this.observer = new IntersectionObserver(this.onSeen.bind(this), {
      // Assume an image will be eventually seen if it is within
      // half the average Desktop vertical screen size:
      // http://gs.statcounter.com/screen-resolution-stats/desktop/north-america
      rootMargin: `540px`,
    });
    this.observer.observe(ReactDOM.findDOMNode(this));
  }

  componentWillUnmount() {
    // Remove observer on unmount
    if (this.observer) {
      this.observer.unobserve(ReactDOM.findDOMNode(this));
    }
    if (this.idleCallbackId) {
      this.props.windowObj.cancelIdleCallback(this.idleCallbackId);
    }
  }

  render() {
    let classNames = `ds-image
      ${this.props.extraClassNames ? ` ${this.props.extraClassNames}` : ``}
      ${this.state && this.state.useTransition ? ` use-transition` : ``}
      ${this.state && this.state.isSeen ? ` loaded` : ``}
    `;

    let img;

    if (this.state && this.state.isSeen) {
      if (
        this.props.optimize &&
        this.props.rawSource &&
        !this.state.optimizedImageFailed
      ) {
        let source;
        let source2x;

        if (this.state && this.state.containerWidth) {
          let baseSource = this.props.rawSource;

          source = this.reformatImageURL(
            baseSource,
            this.state.containerWidth,
            this.state.containerHeight
          );

          source2x = this.reformatImageURL(
            baseSource,
            this.state.containerWidth * 2,
            this.state.containerHeight * 2
          );

          img = (
            <img
              alt={this.props.alt_text}
              crossOrigin="anonymous"
              onError={this.onOptimizedImageError}
              src={source}
              srcSet={`${source2x} 2x`}
            />
          );
        }
      } else if (!this.state.nonOptimizedImageFailed) {
        img = (
          <img
            alt={this.props.alt_text}
            crossOrigin="anonymous"
            onError={this.onNonOptimizedImageError}
            src={this.props.source}
          />
        );
      } else {
        // Remove the img element if both sources fail. Render a placeholder instead.
        img = <div className="broken-image" />;
      }
    }

    return <picture className={classNames}>{img}</picture>;
  }

  onOptimizedImageError() {
    // This will trigger a re-render and the unoptimized 450px image will be used as a fallback
    this.setState({
      optimizedImageFailed: true,
    });
  }

  onNonOptimizedImageError() {
    this.setState({
      nonOptimizedImageFailed: true,
    });
  }
}

DSImage.defaultProps = {
  source: null, // The current source style from Pocket API (always 450px)
  rawSource: null, // Unadulterated image URL to filter through Thumbor
  extraClassNames: null, // Additional classnames to append to component
  optimize: true, // Measure parent container to request exact sizes
  alt_text: null,
  windowObj: window, // Added to support unit tests
};
