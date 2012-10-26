<?xml version="1.0" encoding="utf-8" ?>
<xsl:stylesheet version="1.0" xmlns="http://www.w3.org/1999/xhtml" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:imdi="http://www.mpi.nl/IMDI/Schema/IMDI" xmlns:folia="http://ilk.uvt.nl/folia">

<xsl:output method="html" encoding="UTF-8" omit-xml-declaration="yes" doctype-public="-//W3C//DTD XHTML 1.0 Strict//EN" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd" indent="yes" />

<xsl:template match="/folia:FoLiA">
  <html> 
  <head>
        <meta http-equiv="content-type" content="application/xhtml+xml; charset=utf-8"/>
        <meta name="generator" content="folia2html.xsl" />
        <xsl:choose>
            <xsl:when test="folia:metadata/folia:meta[@id='title']">
                <title>T-scan Viewer :: <xsl:value-of select="folia:metadata/folia:meta[@id='title']" /></title>
            </xsl:when>
            <xsl:when test="folia:metadata[@type='imdi']//imdi:Session/imdi:Title">
                <title>T-scan Viewer ::<xsl:value-of select="folia:metadata[@type='imdi']//imdi:Session/imdi:Title" /></title>
            </xsl:when>
            <xsl:otherwise>
                <title>T-scan Viewer :: <xsl:value-of select="@xml:id" /></title>
            </xsl:otherwise>
        </xsl:choose>
        <style type="text/css">
				body {
					/*background: #222222;*/
					background: #b7c8c7;
					font-family: sans-serif;
					font-size: 12px;
					margin-bottom:240px;
				}
				
				div#textview {
					margin-left: 10px;
					margin-right: auto;
					width: 69%;
				}
				div#header {
					width: 69%;
					background: #74a8a9;
					color: white;
					font-family: monospace;
					border: 2px white solid;					
				}
				div#header h1 {
					display: inline-block;
					font-family: monospace;
					color: white;
					margin-left: 10px;
				}
				div#metrics {
					position: fixed;
					right: 10px;
					top: 10px;
					margin-left: auto;
					margin-right: auto;
					width: 29%;
					background: #c2c9c9;
					padding: 10px;
					border: 2px white solid;	
				}

				div.text {
					width: 700px;
					margin-top: 50px;
					margin-left: auto;
					margin-right: auto;
					padding: 10px;    
					padding-left: 50px;
					padding-right: 50px;
					text-align: left;
					background: white;
					border: 2px solid black;
					min-height: 500px;
				}

				div.div {
					padding-left: 0px;
					padding-top: 10px;
					padding-bottom: 10px;    
				}

				#metadata {
					font-family: sans-serif;
					width: 700px;
					font-size: 90%;
					margin-left: auto;
					margin-right: auto;
					margin-top: 5px;
					margin-bottom: 5px;
					background: #b4d4d1; /*#FCFFD0;*/
					border: 1px solid #628f8b;
					width: 40%;
					padding: 5px;
				}
				#metadata table {
					text-align: left;
				}
				
				#footer {
					margin-top: 100px;
					width: 80%;
					margin-left: auto;
					margin-right: auto;
					font-family: monospace;
					font-size: 10px;
					font-style: italic;
					text-align: center;	
				}

				#text {
					border: 1px solid #628f8b;
					width: 60%; 
					max-width: 1024px;
					background: white;
					padding: 20px;
					padding-right: 100px; 
					margin-top: 5px;
					margin-left: auto; 
					margin-right: auto; 
					color: #222;
				}
				.s {
					display: inline;
					border-left: 1px white solid;
					border-right: 1px white solid;
				}
				.s:hover {
					background: #e7e8f8;
					border-left: 1px blue solid;
					border-right: 1px blue solid;
				}
				.s.selected {
					background: #fffbc3;
					border-left: 1px yellow solid;
					border-right: 1px yellow solid;
				}
				.word { 
					display: inline; 
					color: black; 
					position: relative; 
					text-decoration: none; 
					z-index: 24; 
				}
				#text {
					border: 1px solid #628f8b;
					width: 60%; 
					max-width: 1024px;
					background: white;
					padding: 20px;
					padding-right: 100px; 
					margin-top: 5px;
					margin-left: auto; 
					margin-right: auto; 
					color: #222;
				}

				.word { 
					display: inline; 
					color: black; 
					position: relative; 
					text-decoration: none; 
					z-index: 24;
				}
				
				.t {
					display: inline;
					text-decoration: none;
					z-index: 24;
				}

				th {
					text-align: left;
					padding-left: 6px;
				}
				th .description {
					display:none;
				}
				th:hover .description {
					background: #eee;
					border: 1px #999 solid;
					padding: 10px;
					position: absolute;
					display: block;
					margin-left: 100px;
					font-weight: normal;
					text-align: left;
					opacity: 0.9;
				}

				.word>.attributes { display: none; font-size: 12px; font-weight: normal; }
				.word:hover { 
					/*text-decoration: underline;*/ 
					z-index: 25;
				}
				.word:hover>.t {
					background: #bfc0ed;
					text-decoration: underline;
				}
				.word.selected>.t {
					background: yellow;
					text-decoration: underline;
				}
				
				
				.word:hover>.attributes { 
					display: block; 
					position: absolute;
					width: 320px; 
					font-size: 12px;
					left: 2em; 
					top: 2em;
					background: #b4d4d1; /*#FCFFD0;*/
					opacity: 0.9; filter: alpha(opacity = 90); 
					border: 1px solid #628f8b; 
					padding: 5px; 
					text-decoration: none !important;
				}
				.attributes dt {
					color: #254643;
					font-weight: bold;
				}
				.attributes dd {
					color: black;
					font-family: monospace;
				}
				.attributes .wordid {
					display: inline-block:
					width: 100%;
					font-size: 75%;
					color: #555;
					font-family: monospace;
					text-align: center;
				}
				.event {
					padding: 10px;
					border: 1px solid #4f7d87;
				}
				.gap pre {
					padding: 5px;
					background: #ddd;
					border: 1px dashed red;
				}           
				span.attrlabel {
					display: inline-block;
					color: #254643;
					font-weight: bold;
					width: 90px;				
				}	
				span.attrvalue {
					font-weight: 12px;
					font-family: monospace;
				}
				div#iewarning {
					width: 90%;
					padding: 10px;
					color: red;
					font-size: 16px;
					font-weight: bold;
					text-align: center;					
				}	

				#metrics h2 {
					color: #1b5d5e;
					font-size: 12px;
				}
				.metricslist {
					background: white;
					border: 1px black solid;
					min-width: 80%;
					height: 120px;
					overflow-y: scroll;
					font-family: monospace;
				}
				
				p.selected {
					background: #eee;
				}
        </style>
        <link rel="stylesheet" href="http://code.jquery.com/ui/1.9.1/themes/base/jquery-ui.css" />
        <script src="http://code.jquery.com/jquery-1.8.2.min.js"></script>
        <script src="http://code.jquery.com/ui/1.9.1/jquery-ui.js"></script>
        <script type="text/javascript">
        	<![CDATA[
        	selectedword = null;
        	selectedsentence = null;
        	selectedpar = null;
        	
        	function setmetrics(target, metrics) {        		
        		var s = "<table>";
        		for (var i in metrics) {
        			var label = "";
        			var cls = "";
        			var desc = "";                
        			for (key in metrics[i]) {
        				if (key == "label") {
        					label = metrics[i][key]
        				} else if (key == "desc") {
        					desc = metrics[i][key]
        				} else {
        					cls = key;
        				}
        			}
        			if (label == "") label = cls;
        			if (desc == "") desc = cls;  
        			if (metrics[i][cls]) {
		    				s += "<tr><th>" + label + "<span class=\"description\">" + desc + "</span></th><td>" + metrics[i][cls] + "</td></tr>"; 
		    		}
        		}
        		s += "<tr></tr></table>";
        		$(target).html(s);
        	}
        	
        	function selectword(o) {
        		if (selectedword) {
        			$(selectedword).removeClass('selected');
        		}
        		selectedword = o;
        		$(selectedword).addClass('selected');
        	}
        	
        	function selectsentence(o) {
        		if (selectedsentence) {
        			$(selectedsentence).removeClass('selected');
        		}
        		selectedsentence = o;
        		$(selectedsentence).addClass('selected');
        	}        	

  			function selectpar(o) {
        		if (selectedpar) {
        			$(selectedpar).removeClass('selected');
        		}
        		selectedpar = o;
        		$(selectedpar).addClass('selected');
        	}               	
        	
        	$(document).ready(function(){
        		$('#globalmetrics').resizable();
        		$('#sentencemetrics').resizable();
        		$('#wordmetrics').resizable();
        	});
        	
        	]]>
        </script>
  </head>
    <body>
    	<div id="header">
    		<h1>t-scan</h1> :: Software voor Complexiteits Analyse voor het Nederlands
    	</div>
    
    	<div id="textview">
			<xsl:comment><![CDATA[[if lte IE 10]>
			<div id="iewarning">
				The FoLiA viewer does not work properly with Internet Explorer, please consider upgrading to Mozilla Firefox or Google Chrome instead. 
			</div>
			<![endif]]]></xsl:comment>       
		    <xsl:apply-templates />
		    
		    <div id="footer">
		    	<strong>T-scan</strong> is ontworpen en ontwikkeld door <strong>Rogier Kraf</strong> (Universiteit Utrecht) &amp; <strong>Ko van der Sloot</strong> (Universiteit Tilburg), <a href="http://proycon.github.com/folia">FoLiA</a> interface door Maarten van Gompel (Radboud Universiteit Nijmegen). Supervisie door Antal van den Bosch (Radboud Universiteit Nijmegen) en Henk Pander Maat (Universiteit Utrecht). Dit maakt deel uit van het NWO-project Leesbaarheidsindex Nederlands (LIN). Alle eigen software is gelicenseerd onder de <a href="http://www.gnu.org/licenses/gpl.html">GNU Publieke Licentie v3</a>. 
		    </div>
        </div>
        
        <div id="metrics">
        	<xsl:call-template name="globalmetrics" />
        </div>        
    </body> 
  </html>
</xsl:template>

<xsl:template match="folia:text">
 <div class="text">
 	<xsl:apply-templates />
 </div>
</xsl:template>

<xsl:template match="folia:div">
 <div class="div"> 
   <xsl:apply-templates />
 </div>
</xsl:template>

<xsl:template match="folia:p">
 <p id="{@xml:id}">
   <xsl:attribute name="onclick">
  	selectpar(this);
  	setmetrics('#parmetrics',[<xsl:for-each select="folia:metric">{'<xsl:value-of select="@class" />':'<xsl:value-of select="@value" />','label':'<xsl:call-template name="metriclabel" />','desc':'<xsl:call-template name="metricdescription" />'},</xsl:for-each>]);  	
  </xsl:attribute>
  <xsl:apply-templates />
 </p>
</xsl:template>


<xsl:template match="folia:gap">
 <pre class="gap">
  <xsl:value-of select="folia:content" />
 </pre>
</xsl:template>


<xsl:template match="folia:head">
<xsl:choose>
 <xsl:when test="count(ancestor::folia:div) = 1">
    <h1>
        <xsl:apply-templates />
    </h1>
 </xsl:when>
 <xsl:when test="count(ancestor::folia:div) = 2">
    <h2>
        <xsl:apply-templates />
    </h2>
 </xsl:when> 
 <xsl:when test="count(ancestor::folia:div) = 3">
    <h3>
        <xsl:apply-templates />
    </h3>
 </xsl:when>  
 <xsl:when test="count(ancestor::folia:div) = 4">
    <h4>
        <xsl:apply-templates />
    </h4>
 </xsl:when>  
 <xsl:when test="count(ancestor::folia:div) = 5">
    <h5>
        <xsl:apply-templates />
    </h5>
 </xsl:when>   
 <xsl:otherwise>
    <h6>
        <xsl:apply-templates />
    </h6>
 </xsl:otherwise>
</xsl:choose> 
</xsl:template>

<xsl:template match="folia:list">
<ul>
    <xsl:apply-templates />
</ul>
</xsl:template>

<xsl:template match="folia:listitem">
<li><xsl:apply-templates /></li>
</xsl:template>

<xsl:template match="folia:s">
 <span id="{@xml:id}" class="s">
  <xsl:attribute name="onclick">
  	selectsentence(this);
  	setmetrics('#sentencemetrics',[<xsl:for-each select="folia:metric">{'<xsl:value-of select="@class" />':'<xsl:value-of select="@value" />','label':'<xsl:call-template name="metriclabel" />','desc':'<xsl:call-template name="metricdescription" />'},</xsl:for-each>]);  	
  </xsl:attribute>
  <xsl:apply-templates select=".//folia:w|folia:whitespace|folia:br" />
 </span>
</xsl:template>

<xsl:template match="folia:w">
<xsl:if test="not(ancestor::folia:original) and not(ancestor::folia:suggestion) and not(ancestor::folia:alternative)">
<span id="{@xml:id}" class="word"><xsl:attribute name="onclick">selectword(this); setmetrics('#wordmetrics',[<xsl:for-each select="folia:metric">{'<xsl:value-of select="@class" />':'<xsl:value-of select="@value" />','label':'<xsl:call-template name="metriclabel" />','desc':'<xsl:call-template name="metricdescription" />' },</xsl:for-each>]);</xsl:attribute><span class="t"><xsl:value-of select=".//folia:t[1]"/></span><xsl:call-template name="tokenannotations" /></span>
<xsl:choose>
   <xsl:when test="@space = 'no'"></xsl:when>
   <xsl:otherwise>
    <xsl:text> </xsl:text>
   </xsl:otherwise>
</xsl:choose>
</xsl:if>
</xsl:template>

<xsl:template name="tokenannotations">
 <span class="attributes">
 	<span class="attrlabel">ID</span><span class="attrvalue"><xsl:value-of select="@xml:id" /></span><br />
	<xsl:if test="folia:phon">
        	<span class="attrlabel">Phonetics</span><span class="attrvalue"><xsl:value-of select="folia:phon/@class" /></span><br />
    </xsl:if>
        <xsl:if test="folia:pos">
        	<span class="attrlabel">PoS</span><span class="attrvalue"><xsl:value-of select="folia:pos/@class" /></span><br />
        </xsl:if>
        <xsl:if test="folia:lemma">
			<span class="attrlabel">Lemma</span><span class="attrvalue"><xsl:value-of select="folia:lemma/@class" /></span><br />
        </xsl:if>
        <xsl:if test="folia:sense">
			<span class="attrlabel">Sense</span><span class="attrvalue"><xsl:value-of select="folia:sense/@class" /></span><br />
        </xsl:if>
        <xsl:if test="folia:subjectivity">
			<span class="attrlabel">Subjectivity</span><span class="attrvalue"><xsl:value-of select="folia:subjectivity/@class" /></span><br />
        </xsl:if>
        <xsl:if test="folia:errordetection[@errors='yes']">
			<span class="attrlabel">Error detection</span><span class="attrvalue">Possible errors</span><br />        
        </xsl:if>
        <xsl:if test="folia:correction">
            <xsl:if test="folia:correction/folia:suggestion/folia:t">
            	<span class="attrlabel">Suggestion(s) for text correction</span><span class="attrvalue"><xsl:for-each select="folia:correction/folia:suggestion/folia:t">
                    <em><xsl:value-of select="." /></em><xsl:text> </xsl:text>
                </xsl:for-each></span><br />        
            </xsl:if>
            <xsl:if test="folia:correction/folia:original/folia:t">
            	<span class="attrlabel">Original pre-corrected text</span>
            	<span class="attrvalue">                
                <xsl:for-each select="folia:correction/folia:original/folia:t[1]">
                    <em><xsl:value-of select="." /></em><xsl:text> </xsl:text>
                </xsl:for-each>      
                </span><br />            
            </xsl:if>            
        </xsl:if>
 </span>
</xsl:template>

<xsl:template match="folia:whitespace">
 <br /><br />
</xsl:template>

<xsl:template match="folia:figure">
 <div class="figure">
  <img>
      <xsl:attribute name="src">
        <xsl:value-of select="@src" />
      </xsl:attribute>
      <xsl:attribute name="alt">
        <xsl:value-of select="folia:desc" />
      </xsl:attribute>      
  </img>
  <xsl:if test="folia:caption">
   <div class="caption">
     <xsl:apply-templates select="folia:caption/*" />
   </div>
  </xsl:if>
 </div>
</xsl:template>

<xsl:template name="globalmetrics">
	<h2>Globale Tekstkenmerken</h2>
	
	<div id="globalmetrics" class="metricslist">
		<table>
			<xsl:for-each select="//folia:text/folia:metric">
				<tr>
					<th><xsl:call-template name="metriclabel" /><span class="description"><xsl:call-template name="metricdescription" /></span></th>
					<td><xsl:value-of select="@value" /></td>
				</tr>
			</xsl:for-each>
			<tr></tr>
		</table>
	</div>
	
	<h2>Paragraafkenmerken</h2>
	
	<div id="parmetrics" class="metricslist">

	</div>
	
	<h2>Zinskenmerken</h2>
	
	<div id="sentencemetrics" class="metricslist">

	</div>
	
	<h2>Woordkenmerken</h2>
	
	<div id="wordmetrics" class="metricslist">
	
	</div>

	
</xsl:template>


<xsl:template name="metriclabel">
	<xsl:choose>
	  <!-- BEGIN PRESENTATION ( geen ' gebruiken in de eigenlijke tekst, gebruik &quot; !!) -->
	  <xsl:when test="@class='word_count'">Number of words</xsl:when>
	  <xsl:when test="@class='name_count'">Number of names</xsl:when>
	  <!-- END PRESENTATION -->
	  <xsl:otherwise>
		<xsl:value-of select="@class" />
	  </xsl:otherwise>
	</xsl:choose>
</xsl:template>


<xsl:template name="metricdescription">
	<xsl:choose>
	  <!-- BEGIN PRESENTATION ( geen " of ' gebruiken in de eigenlijke tekst, gebruik &quot; !!) -->
	  <xsl:when test="@class='word_count'">The Number of words blah</xsl:when>
	  <xsl:when test="@class='name_count'">The Number of names blah blah</xsl:when>
	  <!-- END PRESENTATION -->
	  <xsl:otherwise>
		<xsl:value-of select="@class" />
	  </xsl:otherwise>
	</xsl:choose>
</xsl:template>

</xsl:stylesheet>

