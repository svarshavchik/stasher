<?xml version='1.0'?>

<!--

Copyright 2012 Double Precision, Inc.
See COPYING for distribution information.

-->

<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:output method="text" />

  <!--
      "resultsdecl" - declare the results class, the ref-counted class
      that contains all the variables in the result message.

      "resultsdef" - constructor and destructor for the results class.

      "replymsgdecl" - declare a wrapper for resultsdecl. This is the
      reply message that gets deserialized. It consists of a uuid and the
      results message.

      "replymsgdef" - constructors and destructor for the replymsg class.

      "reqdecl" - declare the request class.

      "reqdef" - constructor and destructor for the request class.

      "client" - canned code for clientObj::implObj that receives a dispatched
      request message, sends the request message to the peer, receives the
      deserialized reply message, and acks the request.

      "clientdecl" - client object method declarations.

      "privclasslist" - generate privileged messages' classlist iterator.

      "privdeserialized" - generate privileged messages' deserialization
                           declarations.

      "nonprivclasslist" - generate unprivileged messages' classlist iterator.

      "nonprivdeserialized" - generate unprivileged messages' deserialization
                              declarations.

      "clientclasslist" - generate client side reply message classlist iterator.
      "clientdeserialized" - generate client side deserialization
                           declarations.
  -->
  <xsl:param name="mode" />

  <!-- Emit conditional ifndef/define/endifs in headers -->

  <xsl:template name="ifndef">

    <xsl:param name="text" />

    <xsl:choose>
      <xsl:when test="$mode = 'resultsdef'" />
      <xsl:when test="$mode = 'replymsgdef'" />
      <xsl:when test="$mode = 'reqdef'" />
      <xsl:when test="$mode = 'client'" />
      <xsl:otherwise>
	<xsl:value-of select="$text" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="ifndef_begin">

    <xsl:call-template name="ifndef">
      <xsl:with-param name="text">
	<xsl:text>#ifndef stasher_</xsl:text>
	<xsl:value-of select="$mode" />
	<xsl:text>_H&#10;#define stasher_</xsl:text>
	<xsl:value-of select="$mode" />
	<xsl:text>_H&#10;</xsl:text>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="ifndef_end">
    <xsl:call-template name="ifndef">
      <xsl:with-param name="text">
	<xsl:text>&#10;#endif&#10;</xsl:text>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <!-- Header files -->

  <xsl:template name="msggen">
    <xsl:call-template name="ifndef_begin" />

    <xsl:text>&#10;#include &lt;stasher/namespace.H&gt;&#10;</xsl:text>
    <xsl:value-of select="includes" />

    <xsl:value-of select="privincludes[@module=$mode]" />

    <xsl:text>&#10;STASHER_NAMESPACE_START&#10;&#10;</xsl:text>

    <xsl:choose>
      <xsl:when test="$mode = 'resultsdecl'">

	<!--
	    stasher/results.auto.H

            Define objects that hold each field of a response message
	-->

	<xsl:for-each select="message|clientonlymessage|serveronlymessage">
	  <xsl:text>&#10;//! A response to a server request.&#10;&#10;</xsl:text>
	  <xsl:value-of select="comment" />

	  <xsl:text>&#10;class </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj : virtual public x::obj {&#10;public:&#10;//! Constructor&#10;    </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj();&#10;//! Destructor&#10;    ~</xsl:text>
	  <xsl:value-of select="@name" /><xsl:text>resultsObj() noexcept;&#10;</xsl:text>
	  <xsl:for-each select="results/field">
	    <xsl:apply-templates select="comment" mode="copy" />
	    <xsl:text>&#10;    </xsl:text>
	    <xsl:apply-templates select="decl" mode="copy" />
	    <xsl:text>;&#10;</xsl:text>
	  </xsl:for-each>

	  <!-- Emit serialization function that iterates over each field -->

	  <xsl:text>&#10;//! Serialization function&#10;    template &lt;typename iter_type&gt;&#10;    void serialize(iter_type &amp;iter)&#10;    {&#10;</xsl:text>
	  <xsl:for-each select="results/field">
	    <xsl:text>        iter(</xsl:text>
	    <xsl:value-of select="decl/name" />
	    <xsl:text>);&#10;</xsl:text>
	  </xsl:for-each>

	  <!-- Now, a placeholder that receives the deserialized message -->

	  <xsl:text>    }&#10;&#10;    //! Internal message placeholder&#10;    class recvObj : virtual public x::obj {&#10;&#10;        //! Received message placed here&#10;        x::ptr&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; msg;&#10;&#10;    public:&#10;        //! Constructor&#10;        recvObj();&#10;        //! Destructor&#10;        ~recvObj() noexcept;&#10;&#10;        //! Return the received message&#10;        x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; getmsg();&#10;&#10;        //! Install the received message&#10;        void installmsg(const x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; &amp;msgArg);&#10;    };&#10;};&#10;&#10;//! A reference to the results of a </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text> message&#10;typedef x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>results;&#10;&#10;//! A nullable pointer reference to a request for a </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text> message &#10;typedef x::ptr&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsptr;&#10;&#10;//! A reference to a response to a </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text> message&#10;typedef x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::recvObj&gt; </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>request;&#10;&#10;//! A nullable pointer reference to a response to a </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text> message;&#10;typedef x::ptr&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::recvObj&gt; </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>requestptr;&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  results.auto.C

          Constructors and destructors for result messages
      -->

      <xsl:when test="$mode = 'resultsdef'">
	<xsl:for-each select="message|clientonlymessage|serveronlymessage">
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj()</xsl:text>

	  <xsl:for-each select="results/field[default != '']">
	    <xsl:choose>
	      <xsl:when test="position()=1">
		<xsl:text> : </xsl:text>
	      </xsl:when>

	      <xsl:otherwise>
		<xsl:text>, </xsl:text>
	      </xsl:otherwise>
	    </xsl:choose>
	    <xsl:value-of select="decl/name" />
	    <xsl:text>(</xsl:text>
	    <xsl:value-of select="default" />
	    <xsl:text>)</xsl:text>
	  </xsl:for-each>

	  <xsl:text> {}&#10;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::~</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj() noexcept {}&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  replymsgs.auto.H

          Define the actual response message that gets serialized and
	  deserialized. The response message contains the request message's
	  uuid, then the ref-counted results message, with all the actual
	  fields.

	  The serialization function iterates over the request id, then
	  invokes the result message's serialization function to iterate
	  over all the fields. The constructor instantiates the results
	  mesasge.

      -->
      <xsl:when test="$mode = 'replymsgdecl'">
	<xsl:for-each select="message|serveronlymessage">
	  <xsl:text>&#10;//! A wrapper for a response to the original request message&#10;&#10;</xsl:text>

	  <xsl:value-of select="comment" />

	  <xsl:text>&#10;class </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply {&#10;public:&#10;&#10;    //! Original request uuid&#10;    x::uuid requuid;&#10;&#10;    //! The response message&#10;    </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>results msg;&#10;&#10;    //! Default constructor&#10;    </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply();&#10;&#10; //! Constructor&#10;    </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply(//! Original request uuid&#10;        const x::uuid &amp;requuidArg);&#10;&#10; //! Destructor&#10;    ~</xsl:text>

	  <xsl:value-of select="@name" />
	  <xsl:text><![CDATA[reply() noexcept;
    //! Serialization function
    template<typename iter_type>
    void serialize(iter_type &iter)
    {
        iter(requuid);
        msg->serialize(iter);
    }
};
]]></xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  replymsgs.auto.C

	  Constructors and destructors of received messages.
      -->
      <xsl:when test="$mode = 'replymsgdef'">
	<xsl:for-each select="message|serveronlymessage">
	  <xsl:value-of select="@name" />
	  <xsl:text>reply::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply() : msg(</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>results::create()) {}&#10;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply(const x::uuid &amp;requuidArg) : requuid(requuidArg), msg(</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>results::create()) {}&#10;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply::~</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>reply() noexcept {}&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  requestmsgs.auto.H

	  Declare request messages
      -->

      <xsl:when test="$mode = 'reqdecl'">
	<xsl:for-each select="message|serveronlymessage">
	  <xsl:text>&#10;//! A request from a client to a server&#10;&#10;</xsl:text>
	  <xsl:value-of select="comment" />

	  <xsl:text>&#10;class </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req {&#10;public:&#10;   //! Request uuid&#10;    x::uuid requuid;&#10;</xsl:text>

	  <xsl:for-each select="request/field">
	    <xsl:apply-templates select="comment" mode="copy" />
	    <xsl:text>&#10;    </xsl:text>
	    <xsl:apply-templates select="decl" mode="copy" />
	    <xsl:text>;&#10;</xsl:text>
	  </xsl:for-each>

	  <xsl:text>&#10;    //! Constructor&#10;    </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req();&#10;&#10;    //! Destructor&#10;    ~</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req() noexcept;&#10;&#10;   //! Serialization function&#10;    template&lt;typename iter_type&gt;&#10;    void serialize(iter_type &amp;iter)&#10;    {&#10;        iter(requuid);&#10;</xsl:text>

	  <xsl:for-each select="request/field">
	    <xsl:text>        iter(</xsl:text>
	    <xsl:value-of select="decl/name" />
	    <xsl:text>);&#10;</xsl:text>
	  </xsl:for-each>
	  <xsl:text>    }&#10;};&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  requestmsgs.auto.C

	  Constructors and destructors of request messages
      -->

      <xsl:when test="$mode = 'reqdef'">
	<xsl:for-each select="message|serveronlymessage">
	  <xsl:value-of select="@name" />
	  <xsl:text>req::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req() {}&#10;&#10;</xsl:text>

	  <xsl:value-of select="@name" />
	  <xsl:text>req::~</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req() noexcept {}&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

    </xsl:choose>

    <xsl:text>STASHER_NAMESPACE_END&#10;</xsl:text>

    <xsl:call-template name="ifndef_end" />
  </xsl:template>

  <!-- Emit a class being iterated over in classlist() -->

  <xsl:template name="emitclasslist">
    <xsl:text>iter.template serialize&lt;STASHER_NAMESPACE::</xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>req, deser&lt;STASHER_NAMESPACE::</xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>req&gt; &gt;();&#10;</xsl:text>
  </xsl:template>

  <!--
      Declare and define deserialized() classes on the server side.

      The stock deserialized() method receives the message, constructs the
      response message, invokes the impl() method to populate the response
      message, then writes the response message.

      results/customclass/name, if defines specifies a name of some substitute
      class that serves as a response message, instead of the default response
      message generated by this stylsheet. The custom class is expected to
      incorporate the default response message anyway, so the result message
      still gets defined elsewhere.
  -->
  <xsl:template name="emitdeserialized">
    <xsl:text>&#10;&#10;//! Convenient alias for a request message &#10;&#10;typedef STASHER_NAMESPACE::</xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>req </xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>_req_t;&#10;</xsl:text>

    <xsl:text>&#10;//! A writable object containing the parameters of a response to a request message&#10;&#10;class </xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>_resp_msg_t {&#10;&#10;    //! The actual type of the writable message&#10;    typedef x::ref&lt;STASHER_NAMESPACE::writtenObj&lt;STASHER_NAMESPACE::</xsl:text>

    <xsl:choose>
      <xsl:when test='results/customclass/name != ""'>
	<xsl:value-of select='results/customclass/name' />
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="@name" />
	<xsl:text>reply</xsl:text>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:text>&gt; &gt; msg_t;&#10;&#10;    //! The instantiated message&#10;&#10;    msg_t msg;&#10;&#10;public:&#10;    //! Constructor&#10;    </xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>_resp_msg_t(&#10;        //! Original message's uuid&#10;        const x::uuid &amp;uuid) : msg(msg_t::create(uuid)) {}&#10;&#10;    //! Destructor&#10;    ~</xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>_resp_msg_t() {}&#10;&#10;    //! The actual response object&#10;    typedef STASHER_NAMESPACE::</xsl:text>

    <xsl:choose>
      <xsl:when test='results/customclass/name != ""'>
	<xsl:value-of select='results/customclass/name' />
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="@name" />
	<xsl:text>resultsObj</xsl:text>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:text> resp_t;&#10;&#10;    //! Return the actual response object&#10;    resp_t &amp;getmsg() { return </xsl:text>

    <xsl:choose>
      <xsl:when test='results/customclass/name != ""'>
	<xsl:text>msg->msg</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>*msg->msg.msg</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>; }&#10;&#10;    //! Write the completed message&#10;    void write(STASHER_NAMESPACE::fdobjwriterthreadObj *writer)&#10;    const {&#10;        writer->write(msg);&#10;    }&#10;};&#10;//! Deserialized a request&#10;&#10;</xsl:text>
    <xsl:value-of select="comment" />
    <xsl:text>&#10;void deserialized(const </xsl:text>
    <xsl:value-of select="@name" />
    <xsl:text>_req_t &amp;msg);&#10;</xsl:text>
  </xsl:template>

  <xsl:template match="messages">
    <xsl:text>// AUTOGENERATED -- DO NOT EDIT&#10;&#10;</xsl:text>

    <xsl:choose>

      <!--
	  localprivconnection.classlist.auto.H

	  Code fragment: classlist() for privlocalconnectionObj
      -->

      <xsl:when test="$mode = 'privclasslist'">
	<xsl:for-each select="message[@class='privileged']|serveronlymessage[@class='privileged']">
	  <xsl:text>iter.template serialize&lt;STASHER_NAMESPACE::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req, deser&lt;STASHER_NAMESPACE::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req&gt; &gt;();&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  localconnection.classlist.auto.H

	  Code fragment: classlist() for localconnectionObj
      -->

      <xsl:when test="$mode = 'nonprivclasslist'">
	<xsl:for-each select="message[@class='nonprivileged']|serveronlymessage[@class='nonprivileged']">
	  <xsl:call-template name="emitclasslist" />
	</xsl:for-each>
      </xsl:when>

      <!--
	  client.classlist.auto.H

	  Code fragment: classlist() for clientObj::implObj
      -->

      <xsl:when test="$mode = 'clientclasslist'">
	<xsl:for-each select="message|serveronlymessage">
	  <xsl:text>iter.template serialize&lt;</xsl:text>

	  <xsl:choose>
	    <xsl:when test='results/customclass/name != ""'>
	      <xsl:value-of select='results/customclass/name' />
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="@name" />
	      <xsl:text>reply</xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>

	  <xsl:text>, deser&lt;</xsl:text>

	  <xsl:choose>
	    <xsl:when test='results/customclass/name != ""'>
	      <xsl:value-of select='results/customclass/name' />
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="@name" />
	      <xsl:text>reply</xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>

	  <xsl:text>&gt; &gt;();&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  localprivconnection.deserialized.auto.H

	  Code fragment: deserialized() for privlocalconnectionObj
      -->

      <xsl:when test="$mode = 'privdeserialized'">
	<xsl:for-each select="message[@class='privileged']|serveronlymessage[@class='privileged']">
	  <xsl:call-template name="emitdeserialized" />
	</xsl:for-each>
      </xsl:when>

      <!--
	  localconnection.deserialized.auto.H

	  Code fragment: deserialized() for localconnectionObj
      -->

      <xsl:when test="$mode = 'nonprivdeserialized'">
	<xsl:for-each select="message[@class='nonprivileged']|serveronlymessage[@class='nonprivileged']">
	  <xsl:call-template name="emitdeserialized" />
	</xsl:for-each>
      </xsl:when>

      <!--
	  client.deserialized.auto.H

	  Code fragment: deserialized() for clientObj::implObj
      -->

      <xsl:when test="$mode = 'clientdeserialized'">
	<xsl:for-each select="message|serveronlymessage">
	  <xsl:value-of select="comment" />
	  <xsl:text>&#10;void deserialized(const </xsl:text>

	  <xsl:choose>
	    <xsl:when test='results/customclass/name != ""'>
	      <xsl:value-of select='results/customclass/name' />
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="@name" />
	      <xsl:text>reply</xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>
	  <xsl:text> &amp;msg);&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  client.api.auto.H

	  Generate method declarations for clientObj:

	  1. The method_request() message that submits a request.

	  2. The method() message that calls method_request(), then waits for
	  the request to be processed.

      -->

      <xsl:when test="$mode = 'clientdecl'">
	<xsl:for-each select="message|clientonlymessage">

	  <xsl:text>&#10;//! Send a request to the server&#10;&#10;</xsl:text>
	  <xsl:value-of select="comment" />
	  <xsl:if test="@class='privileged'">
	    <xsl:text>&#10;//! \note&#10;//! This request requires an administrative connection.&#10;</xsl:text>
	  </xsl:if>
	  <xsl:text>&#10;    std::pair&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>request, x::ref&lt;requestObj&gt; &gt; </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>_request(</xsl:text>
	  <xsl:apply-templates select="request/field" mode="args-decl" />
	  <xsl:text>);&#10;&#10;//! Send a request to the server and wait for the response.&#10;&#10;</xsl:text>
	  <xsl:value-of select="comment" />
	  <xsl:if test="@class='privileged'">
	    <xsl:text>&#10;//! \note&#10;//! This request requires an administrative connection.&#10;</xsl:text>
	  </xsl:if>
	  <xsl:text>&#10;    </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>results </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>(</xsl:text>
	  <xsl:apply-templates select="request/field" mode="args-decl" />
	  <xsl:text>);&#10;</xsl:text>
	</xsl:for-each>
      </xsl:when>

      <!--
	  client.auto.C

	  Define method_request() and method() functions for clientObj.

	  Also, define the dispatch() method for clientObj::implObj.
      -->
      <xsl:when test="$mode = 'client'">
	<xsl:for-each select="message|clientonlymessage">
	  <xsl:text>&#10;</xsl:text>

	  <!--
	      Also, stick the constructors and destructors for recvObj
	      placeholders here.
	  -->

	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::recvObj::recvObj() {}&#10;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::recvObj::~recvObj() noexcept {}&#10;x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; </xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::recvObj::getmsg() {std::lock_guard&lt;std::mutex&gt; lock(objmutex); if (msg.null()) return x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt;::create(); return msg;}&#10;void </xsl:text>

	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj::recvObj::installmsg(const x::ref&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>resultsObj&gt; &amp;msgArg) { std::lock_guard&lt;std::mutex&gt; lock(objmutex); msg=msgArg; }&#10;&#10;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>results clientObj::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>(</xsl:text>

	  <xsl:apply-templates select="request/field" mode="args-decl" />

	  <xsl:text>)&#10;{&#10;    std::pair&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>request, client::base::request&gt; req=</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>_request(</xsl:text>
	  <xsl:for-each select="request/field">
	    <xsl:if test="position() != 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="decl/name" />
	  </xsl:for-each>
	  <xsl:text>);&#10;&#10;    req.second->wait();&#10;    return req.first-&gt;getmsg();&#10;}&#10;&#10;std::pair&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>request, client::base::request&gt; clientObj::</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>_request(</xsl:text>

	  <xsl:apply-templates select="request/field" mode="args-decl" />

	  <xsl:text><![CDATA[)
{
    x::ref<x::obj> mcguffin=x::ref<x::obj>::create();
    ]]></xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>request res=</xsl:text>

	  <xsl:value-of select="@name" />
	  <xsl:text>request::create();&#10;&#10;    std::pair&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>request, client::base::request&gt;&#10;        retval(res, client::base::request::create(mcguffin));&#10;&#10;    auto c=conn();&#10;&#10;    if (!c.null()) c-></xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>(</xsl:text>
	  <xsl:for-each select="request/field">
	    <xsl:value-of select="decl/name" />
	    <xsl:text>, </xsl:text>
	  </xsl:for-each>
	  <xsl:text>res, mcguffin);

    return retval;
}
</xsl:text>
	</xsl:for-each>

	<xsl:for-each select="message">
	  <xsl:text>&#10;void clientObj::implObj::dispatch_</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>(</xsl:text>
	  <xsl:call-template name="copy-dispatch-params">
	    <xsl:with-param name="name">
	      <xsl:value-of select="@name" />
	    </xsl:with-param>
	  </xsl:call-template>
	  <xsl:text>)&#10;{&#10;    typedef x::ref&lt;writtenObj&lt;</xsl:text>
	  <xsl:value-of select="@name" />
	  <xsl:text>req&gt; &gt; req_t;&#10;&#10;    req_t req=req_t::create();&#10;&#10;</xsl:text>

	  <xsl:for-each select="request/field">
	    <xsl:text>    req-&gt;msg.</xsl:text>
	    <xsl:value-of select="decl/name" />
	    <xsl:text>=</xsl:text>
	    <xsl:value-of select="decl/name" />
	    <xsl:text>;&#10;</xsl:text>
	  </xsl:for-each>

	  <xsl:text>    writer-&gt;write(req);&#10;    reqs->insert(std::make_pair(req->msg.requuid,&#10;                reqinfo(mcguffin, results)));&#10;}&#10;&#10;</xsl:text>

	  <xsl:text>void clientObj::implObj::deserialized(const </xsl:text>


	  <xsl:choose>
	    <xsl:when test='results/customclass/name != ""'>
	      <xsl:value-of select='results/customclass/name' />
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="@name" />
	      <xsl:text>reply</xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>
	  <xsl:text> &amp;msg)<![CDATA[
{
    reqs_t::iterator p=reqs->find(msg.requuid);

    if (p != reqs->end())
    {
        x::ref<]]></xsl:text>
	  <xsl:value-of select="@name" /><xsl:text><![CDATA[resultsObj::recvObj>(p->second.resp)->installmsg(msg.msg);
        reqs->erase(p);
    }
}
]]></xsl:text>
	</xsl:for-each>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="msggen" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!--
      Enumerate comma-separated list of request or response fields.
  -->
  <xsl:template match="field" mode="args-decl">
    <xsl:if test="position() != 1">
      <xsl:text>, </xsl:text>
    </xsl:if>

    <xsl:choose>
      <xsl:when test="comment">
	<xsl:value-of select="comment" />
	<xsl:text>&#10;</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>&#10;//! Parameter&#10;</xsl:text>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:choose>

      <!--

	  Alternate field declaration for clientObj::request_method(),
	  clientObj::request(), and clientObj::implObj::dispatch()

      -->

      <xsl:when test="(($mode='clientdecl') or ($mode='client')) and clientdecl">
	<xsl:for-each select="clientdecl">
	  <xsl:call-template name="emit-field" />
	</xsl:for-each>
      </xsl:when>

      <xsl:otherwise>
	<xsl:for-each select="decl">
	  <xsl:call-template name="emit-field" />
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="emit-field">

    <xsl:if test="@type = 'class'">
      <xsl:text>const </xsl:text>
    </xsl:if>

    <xsl:if test="@type = 'weakref'">
      <xsl:text>const x::ptr&lt;</xsl:text>
    </xsl:if>

    <xsl:copy>
      <xsl:apply-templates select="node()" mode="args-decl" />
    </xsl:copy>
  </xsl:template>

  <xsl:template name="copy-dispatch-params">
    <xsl:param name="name" />
    <xsl:for-each select="/messages/class/method[@name=$name]/param/decl">
      <xsl:if test="position() != 1">
	<xsl:text>,</xsl:text>
      </xsl:if>
      <xsl:text>&#10;        </xsl:text>
      <xsl:value-of select="node()" />
    </xsl:for-each>
  </xsl:template>
  <xsl:template match="name" mode="args-decl">
    <xsl:if test="../@type = 'weakref'">
      <xsl:text>&gt; &amp;</xsl:text>
    </xsl:if>

    <xsl:if test="../@type = 'class'">
      <xsl:text>&amp;</xsl:text>
    </xsl:if>

    <xsl:copy>
      <xsl:apply-templates mode="args-decl" />
    </xsl:copy>
  </xsl:template>

  <!-- Ignore included client.msgs.xml -->
  <xsl:template match="class" />

  <xsl:template match="@*|node()" mode="copy">
    <xsl:copy>
      <xsl:apply-templates mode="copy" />
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>
