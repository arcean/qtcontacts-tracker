<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template name="caseStats">
    <xsl:param name="allCases" select="true()" />

    <xsl:value-of select="concat(count(.//case[($allCases or @insignificant!='true') and @result='PASS']), ' of ',
                                 count(.//case[($allCases or @insignificant!='true')]), ' passed, ',
                                 count(.//case[($allCases or @insignificant!='true') and @result='FAIL']), ' failed')"/>

    <xsl:if test="count(.//case[($allCases or @insignificant!='true') and @result='FAIL']) > 0">
      <xsl:value-of select="concat(' (', round(count(.//case[($allCases or @insignificant!='true') and @result='FAIL'])
                                           div count(.//case[($allCases or @insignificant!='true')]) * 1000)
                                           div 10, '%)')"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="/testresults">
    <html>
      <head>
        <title>Test Results - <xsl:value-of select="suite/@name"/></title>
        <style type="text/css"><![CDATA[
          *[onclick] {
            cursor: pointer;
          }

          body {
            color: black;
            background: white;
          }

          dt {
            font-weight: bold;
            margin-right: 1em;
          }

          dl.compact>dt {
            width: 14em;
            float: left;
            clear: left;
          }

          dl.compact>dt:after {
            content: ':';
          }

          dd.stream {
            background: black;
            color: silver;
            clear: left;
            margin-left: 0em;
            white-space: pre;
            padding: 0.5em;
          }

          .insignificant {
            font-style: italic;
          }

          .FAIL {
            color: red;
          }
          .FAIL.insignificant {
            color: silver;
          }
          .PASS.insignificant {
            color: green;
          }

          #filters {
            padding: .6em;
            background: #d8e8f8;
            border: 1px solid #b8c8d8;
            border-radius: 1em;
            opacity: 0.95;
            color: #234;
            text-align: center;
            position: fixed;
            right: 1em;
            top: 1em;
          }

          #filters .active {
            border-top: 1px solid;
            border-bottom: 1px solid;
            background: #b8c8d8;
          }

        ]]></style>

        <script type="text/javascript"><![CDATA[
          function show(node) {
            node.style.display = null;
          }

          function hide(node) {
            node.style.display = 'none';
          }

          function toggle(node) {
            if ('none' != node.style.display) {
              hide(node);
            } else {
              show(node);
            }
          }

          function toggleById(id) {
            var elem = document.getElementById(id);

            if (null != elem) {
              toggle(elem);
            }
          }

          function applyToSiblings(node, callback) {
            var tagName = node.tagName;

            while(null != (node = node.nextSibling)) {
              if (node.tagName == tagName) {
                break;
              }

              if (null != node.style) {
                callback(node);
              }
            }
          }

          function toggleSiblings(node) {
            applyToSiblings(node, toggle);
          }

          function hasVisibleTestCases(suiteNode) {
            for(var node = suiteNode.nextSibling; node != null; node = node.nextSibling) {
              if (node.tagName == suiteNode.tagName) {
                break;
              }

              if ('DIV' == node.tagName && node.id.search(/^set-/) == 0) {
                for(node = node.firstChild; node != null; node = node.nextSibling) {
                  if (node.tagName == 'DL' && node.className.search(/\bdetails\b/) >= 0) {
                    for(node = node.firstChild; node != null; node = node.nextSibling) {
                      if (node.tagName == 'DT' && node.style.display != 'none') {
                        return true;
                      }
                    }

                    break;
                  }
                }

                break;
              }
            }

            return false;
          }

          function applyFilter(filterNode) {
            for(var f = document.getElementById('filters').firstChild; f != null; f = f.nextSibling) {
              if (null == f.tagName) {
                continue;
              }

              if (f == filterNode) {
                f.className = "active";
              } else {
                f.className = null;
              }
            }

            var filterText = filterNode.innerHTML;
            var re = (filterText != 'ALL' ? new RegExp('\\b' + filterText + '\\b') : null);

            var testCaseNodes = document.getElementsByTagName('DT');
            for(var i = 0, l = testCaseNodes.length; i < l; ++i) {
              var node = testCaseNodes[i];

              if (node.className.search(/\btestcase\b/) == -1) {
                continue;
              }

              if (null == re || node.className.search(re) >= 0) {
                show(node);
              } else {
                applyToSiblings(node, hide);
                hide(node);
              }
            }

            var testSuiteNodes = document.getElementsByTagName('H2');
            for(var i = 0, l = testSuiteNodes.length; i < l; ++i) {
              var node = testSuiteNodes[i];

              if (node.className.search(/\btestsuite\b/) == -1) {
                continue;
              }

              if (hasVisibleTestCases(node)) {
                show(node);
              } else {
                hide(node);
              }
            }

            window.location.hash = filterText;
          }

          function parseQuery() {
            if (!window.location.hash) {
              return;
            }

            var filterId = 'filter-' + window.location.hash.substr(1).toLowerCase();
            var filterNode = document.getElementById(filterId);

            if (filterNode) {
              applyFilter(filterNode);
            }
          }

          window.addEventListener('load', parseQuery, false);
        ]]></script>
      </head>
      <body>
        <xsl:variable name="significantTestClasses">
          <xsl:if test="count(.//case[@result = 'FAIL' and @insignificant != 'true']) > 0">FAIL</xsl:if>
        </xsl:variable>
        <h1>Test Results - <xsl:value-of select="suite/@name"/></h1>
        <xsl:if test="suite/@description"><p><xsl:copy-of select="suite/@description"/></p></xsl:if>
        <dl class="compact summary">
          <dt>Environment</dt><dd><xsl:value-of select="@environment"/></dd>
          <dt>Hardware Product</dt><dd><xsl:value-of select="@hwproduct"/></dd>
          <dt>Hardware Build</dt><dd><xsl:value-of select="@hwbuild"/></dd>
          <dt>Full Results (all tests)</dt><dd><xsl:call-template name="caseStats"/></dd>
          <dt>Significant Tests</dt><dd class="{$significantTestClasses}"><xsl:call-template name="caseStats"><xsl:with-param name="allCases" select="false"/></xsl:call-template></dd>
          <dt>Start</dt><dd><xsl:value-of select=".//case[position()=1]/step[position()=1]/start"/></dd>
          <dt>End</dt><dd><xsl:value-of select=".//case[position()=last()]/step[position()=last()]/end"/></dd>
        </dl>

        <xsl:for-each select="suite/set">
          <xsl:variable name="setClasses">
            <xsl:choose>
              <xsl:when test="count(.//case[@result = 'FAIL' and @insignificant != 'true']) > 0">FAIL</xsl:when>
              <xsl:when test="count(.//case[@result = 'FAIL' and @insignificant = 'true']) > 0">FAIL insignificant</xsl:when>
              <xsl:when test="count(.//case[@insignificant='true']) = count(.//case)">insignificant</xsl:when>
            </xsl:choose>
          </xsl:variable>

          <h2 onclick="toggleById('set-{@name}')" class="{$setClasses} testsuite"><xsl:value-of select="@name"/></h2>
          <xsl:variable name="style"><xsl:if test="0 = count(.//case[@result='FAIL'])">display: none</xsl:if></xsl:variable>
          <div id="set-{@name}" style="{$style}">
            <dl class="summary compact">
              <dt>Full Results (all tests)</dt><dd><xsl:call-template name="caseStats"/></dd>
              <dt>Significant Tests</dt><dd><xsl:call-template name="caseStats"><xsl:with-param name="allCases" select="false"/></xsl:call-template></dd>
            </dl>

            <dl class="details {@result}">
              <xsl:for-each select="case">
                <xsl:variable name="caseClasses">
                  <xsl:value-of select="@result" />
                  <xsl:if test="'true'=@insignificant"><xsl:text> insignificant</xsl:text></xsl:if>
                </xsl:variable>

                <dt onclick="toggleSiblings(this)" class="{$caseClasses} testcase"><xsl:value-of select="concat(@name, ' - ', @result)"/></dt>
                <dd style="display:none">
                  <dl class="compact">
                    <dt>Description</dt><dd><xsl:value-of select="@description"/></dd>
                    <xsl:for-each select="step">
                      <dt>Start</dt><dd><xsl:value-of select="start/text()"/></dd>
                      <dt>End</dt><dd><xsl:value-of select="end/text()"/></dd>
                      <dt>Output</dt><dd class="stream"><xsl:value-of select="stdout/text()"/></dd>
                      <dt>Errors</dt><dd class="stream"><xsl:value-of select="stderr/text()"/></dd>
                    </xsl:for-each>
                  </dl>
                </dd>
              </xsl:for-each>
            </dl>
          </div>
        </xsl:for-each>

        <div id="filters">
          Filter:<xsl:text>&#160;</xsl:text>
          <span onclick="applyFilter(this)" id="filter-pass">PASS</span><xsl:text>&#160;</xsl:text>
          <span onclick="applyFilter(this)" id="filter-fail">FAIL</span><xsl:text>&#160;</xsl:text>
          <span onclick="applyFilter(this)" id="filter-all" class="active">ALL</span>
        </div>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet>

