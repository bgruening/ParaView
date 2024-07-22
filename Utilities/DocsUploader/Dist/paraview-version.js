!function(n){function t(r){if(e[r])return e[r].exports;var o=e[r]={i:r,l:!1,exports:{}};return n[r].call(o.exports,o,o.exports,t),o.l=!0,o.exports}var e={};t.m=n,t.c=e,t.d=function(n,e,r){t.o(n,e)||Object.defineProperty(n,e,{configurable:!1,enumerable:!0,get:r})},t.n=function(n){var e=n&&n.__esModule?function(){return n.default}:function(){return n};return t.d(e,"a",e),e},t.o=function(n,t){return Object.prototype.hasOwnProperty.call(n,t)},t.p="",t(t.s=0)}([function(n,t,e){(function(t){n.exports=t.PV=e(2)}).call(t,e(1))},function(n,t,e){"use strict";var r,o="function"==typeof Symbol&&"symbol"==typeof Symbol.iterator?function(n){return typeof n}:function(n){return n&&"function"==typeof Symbol&&n.constructor===Symbol&&n!==Symbol.prototype?"symbol":typeof n};r=function(){return this}();try{r=r||Function("return this")()||(0,eval)("this")}catch(n){"object"===("undefined"==typeof window?"undefined":o(window))&&(r=window)}n.exports=r},function(n,t,e){"use strict";function r(n){return n&&n.__esModule?n:{default:n}}function o(n,t){var e=document.querySelector("#titlearea");e.innerHTML='<div class="pv-title-cpp title-line"><div class="title-line"><img class="pv-logo" src="paraview-logo-small.png" />'+n+'</div><div class="langSwitch title-line">'+t+"</div></div>",e.querySelector("select").addEventListener("change",u)}function i(n,t){var e=document.querySelector(".wy-side-nav-search li.version");e.innerHTML='<div class="title-line pv-title-python">'+n+"</div>",e.querySelector("select").addEventListener("change",u);var r=document.createElement("li");r.innerHTML='<div class="pv-title-python pv-switch-python">'+t+"</div>",e.parentElement.appendChild(r)}function a(n){return new Promise(function(t,e){var r=new XMLHttpRequest;r.onreadystatechange=function(n){4===r.readyState&&(200===r.status||0===r.status?t(r.response):e(r,n))},r.open("GET",n,!0),r.responseType="text",r.send()})}function s(n,t,e,r){var o=['<select class="versionSelector '+e+'">'];return n.forEach(function(n){o.push('<option value="'+n.value+'" '+(n.value==t?'selected="selected"':"")+">"+n.label+"</option>")}),o.push("</select>"),o.join("")}function c(n,t){return'<a href="/paraview-docs/'+n+'/cxx" class="cpp title-line pv-lang-link '+("cxx"==t?"active":"")+'"><img alt="C++ documentation" title="C++ documentation" class="lang-logo cpp" src="/paraview-docs/'+d.default+'"/> C++</a><a class="python title-line pv-lang-link '+("python"==t?"active":"")+'" href="/paraview-docs/'+n+'/python"><img alt="Python documentation" title="Python documentation" class="lang-logo python" src="/paraview-docs/'+h.default+'" /> Python</a>'}function l(n,t,e){var r=e||y.exec(window.location.href)[2];return n.replace(y,"paraview-docs/"+t+"/"+r+"/")}function u(n){var t=n.target.value,e=window.location.href,r=l(e,t);r!=e&&(window.location.href=r)}function p(){a("/paraview-docs/versions.json").then(function(n){var t=JSON.parse(n),e=y.exec(window.location.href);if(e){var r=e[2],a=g[r]||"cxx",l=e[1],u=s(t,l,r,a),p=c(l,r);"python"===r?i(u,p):"cxx"===r&&o(u,p)}})}Object.defineProperty(t,"__esModule",{value:!0}),t.updateDropDown=p,e(3);var f=e(8),d=r(f),v=e(9),h=r(v),y=/paraview-docs\/([^\/]+)\/(cxx|python)\//,g={python:"cxx",cxx:"python"};p()},function(n,t,e){var r=e(4);"string"==typeof r&&(r=[[n.i,r,""]]);var o={hmr:!0};o.transform=void 0,o.insertInto=void 0;e(6)(r,o);r.locals&&(n.exports=r.locals)},function(n,t,e){t=n.exports=e(5)(!1),t.push([n.i,'.title-line {\n    display: flex;\n    justify-content: space-between;\n    align-items: center;\n    flex-direction: row;\n}\n\n.pv-switch-python {\n    display: flex;\n    justify-content: space-around;\n    align-items: center;\n    flex-direction: row;\n    margin-bottom: 10px;\n}\n\nimg.cpp {\n    padding: 0 5px;\n}\n\nimg.python {\n    padding: 0 3px;\n}\n\n.pv-lang-link {\n    color: #1C2C35;\n    font-weight: bold;\n    opacity: 0.7;\n}\n\n.pv-lang-link:hover {\n    opacity: 1;\n}\n\n.pv-lang-link.active {\n    opacity: 1;\n}\n\n.pv-title-cpp .pv-lang-link.python {\n    margin-left: 20px;\n}\n\n.pv-title-cpp {\n    padding: 0 20px;\n    height: 56px;\n}\n\n.langSwitch {\n    text-align: right;\n}\n\n.pv-logo {\n    height: 50px;\n    margin-right: 20px;\n}\n\n/* Reset image in title section */\n.wy-side-nav-search img {\n    height: auto;\n    width: auto;\n}\n\n.wy-side-nav-search img {\n    height: 50px;\n}\n\n.pv-title-python img.lang-logo {\n    height: 16px;\n}\n\n.pv-title-cpp .lang-logo {\n    height: 25px;\n}\n\n.versionSelector.cxx {\n    height: 30px;\n    font-family: Roboto, sans-serif;\n    font-size: 14px;\n}\n\n.versionSelector.python {\n    width: 100%;\n    font-size: 14.4px;\n    font-family: "Lato","proxima-nova","Helvetica Neue",Arial,sans-serif;\n}',""])},function(n,t,e){"use strict";function r(n,t){var e=n[1]||"",r=n[3];if(!r)return e;if(t&&"function"==typeof btoa){var i=o(r);return[e].concat(r.sources.map(function(n){return"/*# sourceURL="+r.sourceRoot+n+" */"})).concat([i]).join("\n")}return[e].join("\n")}function o(n){return"/*# sourceMappingURL=data:application/json;charset=utf-8;base64,"+btoa(unescape(encodeURIComponent(JSON.stringify(n))))+" */"}n.exports=function(n){var t=[];return t.toString=function(){return this.map(function(t){var e=r(t,n);return t[2]?"@media "+t[2]+"{"+e+"}":e}).join("")},t.i=function(n,e){"string"==typeof n&&(n=[[null,n,""]]);for(var r={},o=0;o<this.length;o++){var i=this[o][0];"number"==typeof i&&(r[i]=!0)}for(o=0;o<n.length;o++){var a=n[o];"number"==typeof a[0]&&r[a[0]]||(e&&!a[2]?a[2]=e:e&&(a[2]="("+a[2]+") and ("+e+")"),t.push(a))}},t}},function(n,t,e){function r(n,t){for(var e=0;e<n.length;e++){var r=n[e],o=h[r.id];if(o){o.refs++;for(var i=0;i<o.parts.length;i++)o.parts[i](r.parts[i]);for(;i<r.parts.length;i++)o.parts.push(p(r.parts[i],t))}else{for(var a=[],i=0;i<r.parts.length;i++)a.push(p(r.parts[i],t));h[r.id]={id:r.id,refs:1,parts:a}}}}function o(n,t){for(var e=[],r={},o=0;o<n.length;o++){var i=n[o],a=t.base?i[0]+t.base:i[0],s=i[1],c=i[2],l=i[3],u={css:s,media:c,sourceMap:l};r[a]?r[a].parts.push(u):e.push(r[a]={id:a,parts:[u]})}return e}function i(n,t){var e=m(n.insertInto);if(!e)throw new Error("Couldn't find a style target. This probably means that the value for the 'insertInto' parameter is invalid.");var r=w[w.length-1];if("top"===n.insertAt)r?r.nextSibling?e.insertBefore(t,r.nextSibling):e.appendChild(t):e.insertBefore(t,e.firstChild),w.push(t);else if("bottom"===n.insertAt)e.appendChild(t);else{if("object"!=typeof n.insertAt||!n.insertAt.before)throw new Error("[Style Loader]\n\n Invalid value for parameter 'insertAt' ('options.insertAt') found.\n Must be 'top', 'bottom', or Object.\n (https://github.com/webpack-contrib/style-loader#insertat)\n");var o=m(n.insertAt.before,e);e.insertBefore(t,o)}}function a(n){if(null===n.parentNode)return!1;n.parentNode.removeChild(n);var t=w.indexOf(n);t>=0&&w.splice(t,1)}function s(n){var t=document.createElement("style");if(void 0===n.attrs.type&&(n.attrs.type="text/css"),void 0===n.attrs.nonce){var e=u();e&&(n.attrs.nonce=e)}return l(t,n.attrs),i(n,t),t}function c(n){var t=document.createElement("link");return void 0===n.attrs.type&&(n.attrs.type="text/css"),n.attrs.rel="stylesheet",l(t,n.attrs),i(n,t),t}function l(n,t){Object.keys(t).forEach(function(e){n.setAttribute(e,t[e])})}function u(){return e.nc}function p(n,t){var e,r,o,i;if(t.transform&&n.css){if(!(i=t.transform(n.css)))return function(){};n.css=i}if(t.singleton){var l=x++;e=b||(b=s(t)),r=f.bind(null,e,l,!1),o=f.bind(null,e,l,!0)}else n.sourceMap&&"function"==typeof URL&&"function"==typeof URL.createObjectURL&&"function"==typeof URL.revokeObjectURL&&"function"==typeof Blob&&"function"==typeof btoa?(e=c(t),r=v.bind(null,e,t),o=function(){a(e),e.href&&URL.revokeObjectURL(e.href)}):(e=s(t),r=d.bind(null,e),o=function(){a(e)});return r(n),function(t){if(t){if(t.css===n.css&&t.media===n.media&&t.sourceMap===n.sourceMap)return;r(n=t)}else o()}}function f(n,t,e,r){var o=e?"":r.css;if(n.styleSheet)n.styleSheet.cssText=j(t,o);else{var i=document.createTextNode(o),a=n.childNodes;a[t]&&n.removeChild(a[t]),a.length?n.insertBefore(i,a[t]):n.appendChild(i)}}function d(n,t){var e=t.css,r=t.media;if(r&&n.setAttribute("media",r),n.styleSheet)n.styleSheet.cssText=e;else{for(;n.firstChild;)n.removeChild(n.firstChild);n.appendChild(document.createTextNode(e))}}function v(n,t,e){var r=e.css,o=e.sourceMap,i=void 0===t.convertToAbsoluteUrls&&o;(t.convertToAbsoluteUrls||i)&&(r=S(r)),o&&(r+="\n/*# sourceMappingURL=data:application/json;base64,"+btoa(unescape(encodeURIComponent(JSON.stringify(o))))+" */");var a=new Blob([r],{type:"text/css"}),s=n.href;n.href=URL.createObjectURL(a),s&&URL.revokeObjectURL(s)}var h={},y=function(n){var t;return function(){return void 0===t&&(t=n.apply(this,arguments)),t}}(function(){return window&&document&&document.all&&!window.atob}),g=function(n,t){return t?t.querySelector(n):document.querySelector(n)},m=function(n){var t={};return function(n,e){if("function"==typeof n)return n();if(void 0===t[n]){var r=g.call(this,n,e);if(window.HTMLIFrameElement&&r instanceof window.HTMLIFrameElement)try{r=r.contentDocument.head}catch(n){r=null}t[n]=r}return t[n]}}(),b=null,x=0,w=[],S=e(7);n.exports=function(n,t){if("undefined"!=typeof DEBUG&&DEBUG&&"object"!=typeof document)throw new Error("The style-loader cannot be used in a non-browser environment");t=t||{},t.attrs="object"==typeof t.attrs?t.attrs:{},t.singleton||"boolean"==typeof t.singleton||(t.singleton=y()),t.insertInto||(t.insertInto="head"),t.insertAt||(t.insertAt="bottom");var e=o(n,t);return r(e,t),function(n){for(var i=[],a=0;a<e.length;a++){var s=e[a],c=h[s.id];c.refs--,i.push(c)}if(n){r(o(n,t),t)}for(var a=0;a<i.length;a++){var c=i[a];if(0===c.refs){for(var l=0;l<c.parts.length;l++)c.parts[l]();delete h[c.id]}}}};var j=function(){var n=[];return function(t,e){return n[t]=e,n.filter(Boolean).join("\n")}}()},function(n,t,e){"use strict";n.exports=function(n){var t="undefined"!=typeof window&&window.location;if(!t)throw new Error("fixUrls requires window.location");if(!n||"string"!=typeof n)return n;var e=t.protocol+"//"+t.host,r=e+t.pathname.replace(/\/[^\/]*$/,"/");return n.replace(/url\s*\(((?:[^)(]|\((?:[^)(]+|\([^)(]*\))*\))*)\)/gi,function(n,t){var o=t.trim().replace(/^"(.*)"$/,function(n,t){return t}).replace(/^'(.*)'$/,function(n,t){return t});if(/^(#|data:|http:\/\/|https:\/\/|file:\/\/\/|\s*$)/i.test(o))return n;var i;return i=0===o.indexOf("//")?o:0===o.indexOf("/")?e+o:r+o.replace(/^\.\//,""),"url("+JSON.stringify(i)+")"})}},function(n,t,e){n.exports=e.p+"02d66cb83e1f24d23569da0d879dc76e.png"},function(n,t,e){n.exports=e.p+"45f9a58a3045c58f6dc7dc9a3c259dd1.png"}]);
